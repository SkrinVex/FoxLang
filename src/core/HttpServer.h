#pragma once
#include "foxlang/Platform.h"
#include <map>
#include <string>
#include <vector>

// Pieces of HTTP that handlers and the server loop share: URL coding, forms,
// multipart uploads, cookies, MIME types and file replies.
namespace foxlang::http {

std::string percentDecode(const std::string& text, bool plusIsSpace);
std::string percentEncode(const std::string& text);
std::string htmlEscape(const std::string& text);

// Keys and values of a query string or an application/x-www-form-urlencoded body.
// A repeated key keeps its first value.
std::map<std::string, std::string> parseQuery(const std::string& text);

struct Part {
    std::string name, filename, contentType, data;
};
// The parts of a multipart/form-data body; empty when the body is not multipart.
std::vector<Part> parseMultipart(const platform::HttpRequest& request);
// A form field from a urlencoded or multipart body.
std::string formValue(const platform::HttpRequest& request, const std::string& name);
std::map<std::string, std::string> parseCookies(const std::string& header);

std::string mimeType(const std::string& path);
// Puts a file in the reply. False, with the reply untouched, when there is no such file.
bool sendFile(platform::HttpReply& reply, const std::string& path, const std::string& contentType = "");

bool validHeaderText(const std::string& text);

} // namespace foxlang::http
