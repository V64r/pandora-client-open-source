#define NOMINMAX
#include "alt_auth.hh"

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace alt_auth {
namespace {
    struct http_response {
        DWORD status = 0;
        std::string body;
        std::wstring location;
        std::vector<std::wstring> set_cookies;
    };

    struct cookie {
        std::string domain, path, name, value;
        bool secure = true;
    };

    static std::string narrow(const std::wstring& value) {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
        std::string out(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), size, nullptr, nullptr);
        return out;
    }

    static std::wstring widen(const std::string& value) {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0);
        std::wstring out(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), size);
        return out;
    }

    static std::string trim(std::string value) {
        const auto non_space = [](unsigned char c) { return !std::isspace(c); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), non_space));
        value.erase(std::find_if(value.rbegin(), value.rend(), non_space).base(), value.end());
        return value;
    }

    static std::string url_encode(const std::string& value) {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string out;
        for (unsigned char c : value) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out.push_back((char)c);
            else { out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
        }
        return out;
    }

    static std::string url_decode(const std::string& value) {
        std::string out;
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '%' && i + 2 < value.size()) {
                const auto digit = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                const int hi = digit(value[i + 1]), lo = digit(value[i + 2]);
                if (hi >= 0 && lo >= 0) { out.push_back((char)((hi << 4) | lo)); i += 2; continue; }
            }
            out.push_back(value[i] == '+' ? ' ' : value[i]);
        }
        return out;
    }

    static std::string json_escape(const std::string& value) {
        std::string out;
        for (char c : value) {
            if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
            else if (c == '\n') out += "\\n";
            else if (c != '\r') out.push_back(c);
        }
        return out;
    }

    static std::string json_string(const std::string& json, const std::string& key) {
        const std::string marker = "\"" + key + "\"";
        size_t p = json.find(marker);
        if (p == std::string::npos) return {};
        p = json.find(':', p + marker.size());
        if (p == std::string::npos) return {};
        p = json.find('"', p + 1);
        if (p == std::string::npos) return {};
        std::string out;
        for (++p; p < json.size(); ++p) {
            const char c = json[p];
            if (c == '"') break;
            if (c == '\\' && p + 1 < json.size()) {
                const char e = json[++p];
                if (e == 'n') out.push_back('\n'); else if (e == 'r') out.push_back('\r');
                else if (e == 't') out.push_back('\t'); else out.push_back(e);
            } else out.push_back(c);
        }
        return out;
    }

    static std::wstring header_value(HINTERNET request, DWORD query) {
        DWORD size = 0;
        WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size) return {};
        std::wstring value(size / sizeof(wchar_t), L'\0');
        if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) return {};
        while (!value.empty() && value.back() == L'\0') value.pop_back();
        return value;
    }

    static http_response request(const std::wstring& url, const wchar_t* verb,
        const std::wstring& headers = {}, const std::string& body = {}, bool manual_redirect = false) {
        http_response result;
        URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
        wchar_t host[256]{}, path[4096]{};
        parts.lpszHostName = host; parts.dwHostNameLength = 255;
        parts.lpszUrlPath = path; parts.dwUrlPathLength = 4095;
        parts.dwSchemeLength = 1; parts.dwExtraInfoLength = 1;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) return result;

        HINTERNET session = WinHttpOpen(L"Swift-account-manager/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return result;
        WinHttpSetTimeouts(session, 5000, 5000, 15000, 15000);
        HINTERNET connection = WinHttpConnect(session, std::wstring(host, parts.dwHostNameLength).c_str(), parts.nPort, 0);
        const std::wstring target = std::wstring(path, parts.dwUrlPathLength) +
            (parts.lpszExtraInfo ? std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength) : L"");
        HINTERNET req = connection ? WinHttpOpenRequest(connection, verb, target.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
        if (req && manual_redirect) {
            DWORD disable = WINHTTP_DISABLE_REDIRECTS;
            WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
        }
        const BOOL sent = req && WinHttpSendRequest(req,
            headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
            headers.empty() ? 0 : (DWORD)-1L,
            body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
            (DWORD)body.size(), (DWORD)body.size(), 0);
        if (sent && WinHttpReceiveResponse(req, nullptr)) {
            DWORD size = sizeof(result.status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &result.status, &size, WINHTTP_NO_HEADER_INDEX);
            result.location = header_value(req, WINHTTP_QUERY_LOCATION);
            DWORD index = 0;
            while (true) {
                DWORD bytes = 0;
                WinHttpQueryHeaders(req, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX,
                    nullptr, &bytes, &index);
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;
                std::wstring value(bytes / sizeof(wchar_t), L'\0');
                if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX,
                    value.data(), &bytes, &index)) break;
                while (!value.empty() && value.back() == L'\0') value.pop_back();
                result.set_cookies.push_back(std::move(value));
            }
            static constexpr size_t max_response_bytes = 4 * 1024 * 1024;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(req, &available) && available) {
                const size_t old = result.body.size();
                if (old >= max_response_bytes) break;
                const DWORD to_read = static_cast<DWORD>((std::min)(
                    static_cast<size_t>(available), max_response_bytes - old));
                result.body.resize(old + to_read);
                DWORD received = 0;
                if (!WinHttpReadData(req, result.body.data() + old, to_read, &received)) break;
                result.body.resize(old + received);
                if (received == 0 || to_read < available) break;
            }
        }
        if (req) WinHttpCloseHandle(req);
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    static result profile_from_token(const std::string& token, method type, const std::string& refresh = {}) {
        const auto response = request(L"https://api.minecraftservices.com/minecraft/profile", L"GET",
            L"Authorization: Bearer " + widen(token) + L"\r\nAccept: application/json\r\n");
        result out;
        if (response.status != 200) { out.error = "Minecraft rejected the access token (HTTP " + std::to_string(response.status) + ")."; return out; }
        out.value.username = json_string(response.body, "name");
        out.value.uuid = json_string(response.body, "id");
        if (out.value.username.empty() || out.value.uuid.empty()) { out.error = "Minecraft profile response was incomplete."; return out; }
        out.value.type = type; out.value.access_token = token; out.value.refresh_credential = refresh; out.success = true;
        return out;
    }

    static result microsoft_to_minecraft(const std::string& msa, method type,
        const std::string& refresh, status_callback status) {
        if (status) status("Acquiring Xbox Live token...");
        const std::string xbl_body = "{\"Properties\":{\"AuthMethod\":\"RPS\",\"SiteName\":\"user.auth.xboxlive.com\",\"RpsTicket\":\"t=" + json_escape(msa) + "\"},\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\"}";
        auto xbl = request(L"https://user.auth.xboxlive.com/user/authenticate", L"POST",
            L"Content-Type: application/json\r\nAccept: application/json\r\nX-Xbl-Contract-Version: 1\r\n", xbl_body);
        if (xbl.status != 200) return { false, {}, "Xbox Live authentication failed (HTTP " + std::to_string(xbl.status) + ")." };
        const std::string xbl_token = json_string(xbl.body, "Token");
        const std::string uhs = json_string(xbl.body, "uhs");
        if (xbl_token.empty() || uhs.empty()) return { false, {}, "Xbox Live response was incomplete." };

        if (status) status("Authorizing Xbox XSTS...");
        const std::string xsts_body = "{\"Properties\":{\"SandboxId\":\"RETAIL\",\"UserTokens\":[\"" + json_escape(xbl_token) + "\"]},\"RelyingParty\":\"rp://api.minecraftservices.com/\",\"TokenType\":\"JWT\"}";
        auto xsts = request(L"https://xsts.auth.xboxlive.com/xsts/authorize", L"POST",
            L"Content-Type: application/json\r\nAccept: application/json\r\nX-Xbl-Contract-Version: 1\r\n", xsts_body);
        if (xsts.status != 200) return { false, {}, "XSTS authorization failed (HTTP " + std::to_string(xsts.status) + ")." };
        const std::string xsts_token = json_string(xsts.body, "Token");
        if (xsts_token.empty()) return { false, {}, "XSTS response did not contain a token." };

        if (status) status("Acquiring Minecraft token...");
        const std::string mc_body = "{\"identityToken\":\"XBL3.0 x=" + json_escape(uhs) + ";" + json_escape(xsts_token) + "\"}";
        auto mc = request(L"https://api.minecraftservices.com/authentication/login_with_xbox", L"POST",
            L"Content-Type: application/json\r\nAccept: application/json\r\n", mc_body);
        if (mc.status != 200) return { false, {}, "Minecraft authentication failed (HTTP " + std::to_string(mc.status) + ")." };
        const std::string mc_token = json_string(mc.body, "access_token");
        if (mc_token.empty()) return { false, {}, "Minecraft response did not contain an access token." };
        if (status) status("Loading Minecraft profile...");
        return profile_from_token(mc_token, type, refresh);
    }

    static std::string extract_url_token(const std::wstring& location) {
        const std::string text = narrow(location);
        size_t p = text.find("access_token=");
        if (p == std::string::npos) return {};
        p += 13; size_t e = text.find('&', p);
        return url_decode(text.substr(p, e == std::string::npos ? e : e - p));
    }

    static bool domain_matches(std::string host, std::string domain) {
        std::transform(host.begin(), host.end(), host.begin(), ::tolower);
        std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);
        if (!domain.empty() && domain[0] == '.') domain.erase(domain.begin());
        return host == domain || (host.size() > domain.size() && host.compare(host.size() - domain.size(), domain.size(), domain) == 0);
    }

    static std::vector<cookie> parse_cookies(const std::string& content) {
        std::vector<cookie> out;
        std::istringstream lines(content); std::string line;
        while (std::getline(lines, line)) {
            line = trim(line); if (line.empty()) continue;
            if (line.rfind("#HttpOnly_", 0) == 0) line.erase(0, 10);
            if (line[0] == '#') continue;
            std::vector<std::string> fields; std::stringstream row(line); std::string field;
            while (std::getline(row, field, '\t')) fields.push_back(field);
            if (fields.size() >= 7) out.push_back({ fields[0], fields[2], fields[5], fields[6], fields[3] == "TRUE" });
        }
        if (!out.empty()) return out;
		// Browser exporters don't guarantee property order. Parse each flat cookie
		// object independently instead of relying on domain/name/value ordering.
		for (size_t begin = content.find('{'); begin != std::string::npos;) {
			const size_t end = content.find('}', begin + 1);
			if (end == std::string::npos) break;
			const std::string object = content.substr(begin, end - begin + 1);
			const std::string name = json_string(object, "name");
			const std::string value = json_string(object, "value");
			std::string domain = json_string(object, "domain");
			if (domain.empty()) domain = json_string(object, "host");
			std::string path = json_string(object, "path");
			if (!name.empty() && !value.empty() && !domain.empty())
				out.push_back({ domain, path.empty() ? "/" : path, name, value, true });
			begin = content.find('{', end + 1);
		}
        if (!out.empty()) return out;
        std::stringstream loose(content); std::string part;
        while (std::getline(loose, part, ';')) {
            const size_t eq = part.find('='); if (eq == std::string::npos) continue;
            const std::string name = trim(part.substr(0, eq)), value = trim(part.substr(eq + 1));
            if (!name.empty() && !value.empty()) out.push_back({ ".live.com", "/", name, value, true });
        }
        return out;
    }

    static std::string cookie_header(const std::vector<cookie>& cookies, const std::string& host) {
        std::string out;
        for (const auto& c : cookies) if (domain_matches(host, c.domain)) {
            if (!out.empty()) out += "; "; out += c.name + "=" + c.value;
        }
        return out;
    }

	static void merge_set_cookies(std::vector<cookie>& cookies,
	                              const std::vector<std::wstring>& headers,
	                              const std::string& request_host) {
		for (const auto& wide : headers) {
			const std::string text = narrow(wide);
			std::stringstream parts(text); std::string part;
			if (!std::getline(parts, part, ';')) continue;
			const size_t eq = part.find('='); if (eq == std::string::npos) continue;
			cookie parsed{ request_host, "/", trim(part.substr(0, eq)), trim(part.substr(eq + 1)), false };
			while (std::getline(parts, part, ';')) {
				part = trim(part); const size_t attr_eq = part.find('=');
				std::string key = trim(part.substr(0, attr_eq));
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);
				const std::string value = attr_eq == std::string::npos ? "" : trim(part.substr(attr_eq + 1));
				if (key == "domain" && !value.empty()) parsed.domain = value;
				else if (key == "path" && !value.empty()) parsed.path = value;
				else if (key == "secure") parsed.secure = true;
			}
			auto existing = std::find_if(cookies.begin(), cookies.end(), [&](const cookie& c) {
				return c.domain == parsed.domain && c.path == parsed.path && c.name == parsed.name;
			});
			if (existing == cookies.end()) cookies.push_back(std::move(parsed)); else *existing = std::move(parsed);
		}
	}

	static std::wstring resolve_redirect(const std::wstring& current, const std::wstring& location) {
		if (location.rfind(L"http://", 0) == 0 || location.rfind(L"https://", 0) == 0) return location;
		const size_t scheme_end = current.find(L"://");
		if (scheme_end == std::wstring::npos) return {};
		const size_t path_start = current.find(L'/', scheme_end + 3);
		const std::wstring origin = path_start == std::wstring::npos ? current : current.substr(0, path_start);
		if (location.rfind(L"//", 0) == 0) return current.substr(0, scheme_end + 1) + location;
		if (!location.empty() && location[0] == L'/') return origin + location;
		const size_t query = current.find_first_of(L"?#");
		std::wstring base = current.substr(0, query);
		const size_t slash = base.find_last_of(L'/');
		return (slash == std::wstring::npos ? origin + L"/" : base.substr(0, slash + 1)) + location;
	}

}

void secure_clear(std::string& value) {
    if (!value.empty()) SecureZeroMemory(value.data(), value.size());
    value.clear(); value.shrink_to_fit();
}

std::vector<std::string> split_credentials(const std::string& input, method type) {
    std::vector<std::string> out;
    std::regex jwt("eyJ[A-Za-z0-9_-]*\\.eyJ[A-Za-z0-9_-]*\\.[A-Za-z0-9_-]*");
    if (type == method::access_token) {
        for (std::sregex_iterator i(input.begin(), input.end(), jwt), end; i != end; ++i) out.push_back(i->str());
        if (!out.empty()) return out;
    }
    std::istringstream lines(input); std::string line;
    while (std::getline(lines, line)) {
        line = trim(line); if (line.empty()) continue;
        if (type == method::refresh_token) {
            const size_t p = line.find("M.C"); if (p != std::string::npos) line = line.substr(p);
        }
        const size_t space = line.find_first_of(" \t|"); if (space != std::string::npos) line = line.substr(0, space);
        if (line.size() >= 20 && std::find(out.begin(), out.end(), line) == out.end()) out.push_back(line);
    }
    return out;
}

result login_access_token(const std::string& token, status_callback status) {
    if (status) status("Validating Minecraft access token...");
    return profile_from_token(trim(token), method::access_token);
}

result login_refresh_token(const std::string& token, status_callback status) {
    if (status) status("Refreshing Microsoft OAuth token...");
    const std::string clean = trim(token);
    const std::string form = "client_id=00000000402b5328&refresh_token=" + url_encode(clean) +
        "&grant_type=refresh_token&scope=" + url_encode("service::user.auth.xboxlive.com::MBI_SSL");
    auto response = request(L"https://login.live.com/oauth20_token.srf", L"POST",
        L"Content-Type: application/x-www-form-urlencoded\r\nAccept: application/json\r\n", form);
    if (response.status != 200) return { false, {}, "Microsoft refresh failed (HTTP " + std::to_string(response.status) + ")." };
    const std::string msa = json_string(response.body, "access_token");
    const std::string rotated = json_string(response.body, "refresh_token");
    if (msa.empty()) return { false, {}, "Microsoft did not return an access token." };
    return microsoft_to_minecraft(msa, method::refresh_token, rotated.empty() ? clean : rotated, status);
}

result login_cookie_text(const std::string& content, status_callback status) {
    if (status) status("Reading browser cookies...");
    auto cookies = parse_cookies(content);
    if (cookies.empty()) return { false, {}, "No supported cookies were found." };
    const std::wstring starts[] = {
        L"https://login.live.com/oauth20_authorize.srf?redirect_uri=https://sisu.xboxlive.com/connect/oauth/XboxLive&response_type=token&client_id=000000004420578E&scope=XboxLive.Signin%20XboxLive.offline_access&prompt=none",
        L"https://login.live.com/oauth20_authorize.srf?client_id=00000000402b5328&redirect_uri=https%3A%2F%2Flogin.live.com%2Foauth20_desktop.srf&response_type=token&scope=service%3A%3Auser.auth.xboxlive.com%3A%3AMBI_SSL&prompt=none"
    };
    if (status) status("Authenticating with Microsoft cookies...");
    for (const auto& start : starts) {
        std::wstring current = start;
        for (int hop = 0; hop < 12; ++hop) {
            URL_COMPONENTS p{}; p.dwStructSize = sizeof(p); wchar_t host[256]{};
            p.lpszHostName = host; p.dwHostNameLength = 255;
            if (!WinHttpCrackUrl(current.c_str(), 0, 0, &p)) break;
            const std::string host8 = narrow(std::wstring(host, p.dwHostNameLength));
            const std::string ch = cookie_header(cookies, host8);
            auto response = request(current, L"GET", widen(ch.empty() ? "" : "Cookie: " + ch + "\r\n") +
                L"Accept: */*\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) Swift/1.0\r\n", {}, true);
			merge_set_cookies(cookies, response.set_cookies, host8);
            const std::string msa = extract_url_token(response.location);
            if (!msa.empty()) {
                std::string serialized;
                for (const auto& c : cookies) serialized += c.domain + "\t" + c.path + "\t" + c.name + "\t" + c.value + "\n";
                auto out = microsoft_to_minecraft(msa, method::cookie, serialized, status);
                secure_clear(serialized);
                return out;
            }
            if (response.location.empty()) break;
			current = resolve_redirect(current, response.location);
			if (current.empty()) break;
        }
    }
    return { false, {}, "Cookie authentication failed. The cookies may be expired or incomplete." };
}
}
