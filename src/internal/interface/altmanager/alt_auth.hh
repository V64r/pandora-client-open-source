#pragma once

#include <functional>
#include <string>
#include <vector>

namespace alt_auth {
    enum class method { cookie, access_token, refresh_token };

    struct account {
        method type = method::access_token;
        std::string username;
        std::string uuid;
        std::string access_token;
        std::string refresh_credential;
    };

    struct result {
        bool success = false;
        account value;
        std::string error;
    };

    using status_callback = std::function<void(const std::string&)>;

    std::vector<std::string> split_credentials(const std::string& input, method type);
    result login_access_token(const std::string& token, status_callback status = {});
    result login_refresh_token(const std::string& token, status_callback status = {});
    result login_cookie_text(const std::string& content, status_callback status = {});
    void secure_clear(std::string& value);
}
