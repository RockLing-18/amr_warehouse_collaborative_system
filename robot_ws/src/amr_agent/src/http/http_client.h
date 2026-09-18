#pragma once
#include <string>
#include <vector>

namespace amr_agent
{

struct HttpResponseRet
{
	bool succeed = false;       // 是否成功，当false时，errMsg有效
	std::string body;   // 成功时返回的消息体
	std::string errMsg; // 错误时的错误信息
	
    std::vector<uint8_t> data; // 文件/二进制
    std::string content_type;
};

class HttpClient 
{
public:
	HttpClient(const std::string& host, int port = 80);

	// GET
	HttpResponseRet get(const std::string& path);

	// 本质是GET
	HttpResponseRet download(const std::string& path);

	// POST
	HttpResponseRet post(const std::string& path, const std::string& body, const std::string& content_type = "application/json");

	// PUT
	HttpResponseRet put(const std::string& path, const std::string& body, const std::string& content_type = "application/json");

	// DELETE
	HttpResponseRet del(const std::string& path);

private:
	HttpResponseRet request(const std::string& method, const std::string& path,
		const std::string& body,
		const std::string& content_type = "application/json");

private:
	std::string m_host;
	int m_port;
};

}