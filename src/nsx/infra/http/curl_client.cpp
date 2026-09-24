// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/infra/http/curl_client.hpp"

#include <cstdio>
#include <variant>

#include <curl/curl.h>

#include "nsx/core/version/version.hpp"
#include "nsx/infra/http/ca_bundle.hpp"

namespace nsx::infra {

namespace {

using core::Sha256;

constexpr long kConnectTimeoutSeconds = 8;
constexpr long kMetadataTimeoutSeconds = 20;
constexpr long kMaxRedirects = 5;

// A large download has no fixed deadline - a slow console on slow Wi-Fi is
// normal - but a stalled one must not hang forever. Below 1 KiB/s for 60
// seconds counts as stalled.
constexpr long kLowSpeedBytesPerSecond = 1024;
constexpr long kLowSpeedSeconds = 60;

/// Apply the security posture every request shares.
///
/// This function is the whole reason a CurlClient exists rather than callers
/// touching curl directly: there is exactly one place that decides how a
/// connection is secured, so there is exactly one place to audit.
void applySecurity(CURL* curl)
{
    // Peer AND host verification, always, on every request. This is the line
    // the predecessor crossed (download.cpp:140-141, 198-199, 353-354,
    // 472-473) and tools/lint/forbid_insecure_curl.sh exists to stop us
    // crossing it again.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // WHERE THE TRUST ANCHOR COMES FROM
    // ---------------------------------
    // devkitPro ships curl 7.69.1 built against libnx's `ssl` service - the
    // firmware's own TLS stack - not mbedTLS. Verification therefore runs
    // against the trust store the console itself uses, maintained by system
    // updates.
    //
    // CURLOPT_CAINFO_BLOB only exists from curl 7.77.0, so the bundle embedded
    // by tools/cacert/pem_to_header.py cannot be handed over on this toolchain.
    // It stays compiled in and is used automatically the moment devkitPro ships
    // a curl that accepts it - the check below is a version gate, not a
    // fallback, and there is no path where verification is weakened.
    //
    // See ADR-0016, which supersedes the mbedTLS reasoning in ADR-0006.
#if defined(CURLOPT_CAINFO_BLOB) && LIBCURL_VERSION_NUM >= 0x074D00
    static const curl_blob caBlob{const_cast<char*>(kCaBundlePem.data()), kCaBundlePem.size(),
                                  CURL_BLOB_NOCOPY};
    curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &caBlob);
#endif

    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    // https only, including after a redirect. A server that redirects to
    // plaintext is not one to follow.
#if defined(CURLOPT_PROTOCOLS_STR)
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, kMaxRedirects);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);  // we inspect the status ourselves

    // GitHub rejects requests without one.
    static const std::string agent{core::version::kUserAgent};
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
}

/// Map a curl result to something the UI can act on.
///
/// The interesting case is clock skew. The Switch RTC is frequently wrong, and
/// a wrong clock makes certificate date validation fail - which surfaces as a
/// generic verification failure. Distinguishing it is what lets the app say
/// "set your console clock" instead of leaving the user with an unexplained
/// refusal, and it is very likely why the predecessor gave up on verification
/// altogether.
HttpError classify(CURLcode code, CURL* curl)
{
    switch (code) {
        case CURLE_OK:
            return HttpError::None;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
            return HttpError::DnsFailure;
        case CURLE_COULDNT_CONNECT:
            return HttpError::ConnectFailed;
        case CURLE_OPERATION_TIMEDOUT:
            return HttpError::Timeout;
        case CURLE_ABORTED_BY_CALLBACK:
            return HttpError::Aborted;
        case CURLE_WRITE_ERROR:
            return HttpError::WriteFailed;
        case CURLE_UNSUPPORTED_PROTOCOL:
        case CURLE_URL_MALFORMAT:
            return HttpError::InsecureUrl;
        case CURLE_FILESIZE_EXCEEDED:
            return HttpError::TooLarge;
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_CONNECT_ERROR: {
            long verifyResult = 0;
            if (curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verifyResult) == CURLE_OK &&
                verifyResult != 0) {
                // mbedTLS reports expired and not-yet-valid through the same
                // verify-result channel; either points at the console clock far
                // more often than at a genuinely bad certificate.
                constexpr long kBadCertExpired = 0x01;
                constexpr long kBadCertFuture = 0x08;
                if ((verifyResult & (kBadCertExpired | kBadCertFuture)) != 0) {
                    return HttpError::TlsClockSkew;
                }
            }
            return HttpError::TlsVerifyFailed;
        }
        default:
            return HttpError::Internal;
    }
}

HttpError classifyStatus(long status)
{
    if (status >= 200 && status < 400) {
        return HttpError::None;
    }
    if (status == 403 || status == 429) {
        return HttpError::RateLimited;
    }
    return HttpError::HttpStatus;
}

struct MemorySink
{
    std::string data;
    std::uint64_t limit{};
    bool overflowed{};
};

std::size_t writeToMemory(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
    auto* sink = static_cast<MemorySink*>(userdata);
    const std::size_t bytes = size * nmemb;

    if (sink->data.size() + bytes > sink->limit) {
        sink->overflowed = true;
        return 0;  // signals CURLE_WRITE_ERROR
    }
    sink->data.append(ptr, bytes);
    return bytes;
}

std::size_t captureHeader(char* buffer, std::size_t size, std::size_t nitems, void* userdata)
{
    auto* etag = static_cast<std::string*>(userdata);
    const std::size_t bytes = size * nitems;
    const std::string_view line(buffer, bytes);

    constexpr std::string_view kEtag = "etag:";
    if (line.size() > kEtag.size()) {
        std::string lowered;
        lowered.reserve(kEtag.size());
        for (std::size_t i = 0; i < kEtag.size(); ++i) {
            lowered.push_back(
                static_cast<char>((line[i] >= 'A' && line[i] <= 'Z') ? (line[i] + 32) : line[i]));
        }
        if (lowered == kEtag) {
            std::string_view value = line.substr(kEtag.size());
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.remove_suffix(1);
            }
            *etag = std::string(value);
        }
    }
    return bytes;
}

/// Destination for a verified download: writes to a file and hashes the same
/// bytes on the way past. One pass - the update path never re-reads a file it
/// just wrote.
struct FileSink
{
    FILE* file{};
    Sha256 hasher;
    std::uint64_t written{};
    std::uint64_t limit{};
    bool failed{};
};

std::size_t writeToFile(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
    auto* sink = static_cast<FileSink*>(userdata);
    const std::size_t bytes = size * nmemb;

    // Refuse to exceed the size the manifest promised. A server that keeps
    // sending is not one to keep listening to.
    if (sink->written + bytes > sink->limit) {
        sink->failed = true;
        return 0;
    }
    if (std::fwrite(ptr, 1, bytes, sink->file) != bytes) {
        sink->failed = true;
        return 0;
    }

    sink->hasher.update(ptr, bytes);
    sink->written += bytes;
    return bytes;
}

int reportProgress(void* userdata, curl_off_t total, curl_off_t received, curl_off_t, curl_off_t)
{
    auto* cb = static_cast<const ProgressCallback*>(userdata);
    if (cb == nullptr || !*cb) {
        return 0;
    }
    Progress p;
    p.received = static_cast<std::uint64_t>(received);
    p.total = static_cast<std::uint64_t>(total);
    return (*cb)(p) ? 0 : 1;  // non-zero aborts
}

}  // namespace

struct CurlClient::Impl
{
    bool globalInitialised{};
};

CurlClient::CurlClient() : m_impl(new Impl)
{
    m_impl->globalInitialised = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

CurlClient::~CurlClient()
{
    if (m_impl != nullptr && m_impl->globalInitialised) {
        curl_global_cleanup();
    }
    delete m_impl;
}

core::Result<Response, HttpError> CurlClient::get(const std::string& url, std::uint64_t maxBytes,
                                                  std::string_view ifNoneMatch)
{
    using R = core::Result<Response, HttpError>;

    // Refused here, by us, before a socket is opened - not left to the server.
    if (!isHttpsUrl(url)) {
        return R::err(HttpError::InsecureUrl);
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return R::err(HttpError::Internal);
    }

    MemorySink sink;
    sink.limit = maxBytes;
    std::string etag;

    applySecurity(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kMetadataTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToMemory);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, captureHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &etag);

    curl_slist* headers = nullptr;
    std::string conditional;
    if (!ifNoneMatch.empty()) {
        conditional = "If-None-Match: " + std::string(ifNoneMatch);
        headers = curl_slist_append(headers, conditional.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const CURLcode code = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    const HttpError transport = classify(code, curl);

    if (headers != nullptr) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);

    if (transport != HttpError::None) {
        // An overflow surfaces as a write error; report the real cause.
        if (sink.overflowed) {
            return R::err(HttpError::TooLarge);
        }
        return R::err(transport);
    }
    if (const HttpError statusError = classifyStatus(status); statusError != HttpError::None) {
        return R::err(statusError);
    }

    Response out;
    out.status = status;
    out.body = std::move(sink.data);
    out.etag = std::move(etag);
    return R::ok(std::move(out));
}

core::Result<std::monostate, HttpError> CurlClient::download(const std::string& url,
                                                             const std::string& destPath,
                                                             const ExpectedArtifact& expected,
                                                             const ProgressCallback& onProgress)
{
    using R = core::Result<std::monostate, HttpError>;

    if (!isHttpsUrl(url)) {
        return R::err(HttpError::InsecureUrl);
    }
    if (expected.size == 0) {
        // Without an expected size there is no truncation check and no ceiling
        // on what a server may send. The manifest always provides one.
        return R::err(HttpError::Internal);
    }

    const std::string partPath = destPath + ".part";

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return R::err(HttpError::Internal);
    }

    FileSink sink;
    sink.limit = expected.size;
    sink.file = std::fopen(partPath.c_str(), "wb");
    if (sink.file == nullptr) {
        curl_easy_cleanup(curl);
        return R::err(HttpError::WriteFailed);
    }

    applySecurity(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);

    // No hard timeout on a large transfer - a slow console on slow Wi-Fi is
    // normal - but a stalled one must not hang forever.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedBytesPerSecond);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kLowSpeedSeconds);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, onProgress ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, reportProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &onProgress);

    const CURLcode code = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    const HttpError transport = classify(code, curl);

    const bool flushed = std::fflush(sink.file) == 0;
    std::fclose(sink.file);
    curl_easy_cleanup(curl);

    auto discard = [&](HttpError e) {
        std::remove(partPath.c_str());
        return R::err(e);
    };

    if (transport != HttpError::None) {
        return discard(sink.failed && sink.written >= sink.limit ? HttpError::TooLarge : transport);
    }
    if (!flushed || sink.failed) {
        return discard(HttpError::WriteFailed);
    }
    if (const HttpError statusError = classifyStatus(status); statusError != HttpError::None) {
        return discard(statusError);
    }

    // Size first: free, and it catches a truncated transfer without hashing
    // megabytes to discover it.
    if (sink.written != expected.size) {
        return discard(HttpError::SizeMismatch);
    }

    const Sha256::Digest actual = sink.hasher.finish();
    if (!core::digestsEqual(actual, expected.sha256)) {
        // Never promote an unverified file. A .part is not something that can
        // be executed, extracted or chainloaded, and this is what keeps it so.
        return discard(HttpError::DigestMismatch);
    }

    // Only now does it get a name anything else will act on. FatFs cannot
    // rename onto an existing file.
    std::remove(destPath.c_str());
    if (std::rename(partPath.c_str(), destPath.c_str()) != 0) {
        return discard(HttpError::WriteFailed);
    }

    return R::ok(std::monostate{});
}

}  // namespace nsx::infra
