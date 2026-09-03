#pragma once
#include <string>

struct HttpResponseRet
{
	bool succeed = false;       // 是否成功，当false时，errMsg有效
	std::string body;   // 成功时返回的消息体
	std::string errMsg; // 错误时的错误信息
};

class HttpClient 
{
public:
	HttpClient(const std::string& host, int port = 443);

	// GET
	HttpResponseRet get(const std::string& path);

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

/*
try
{
	HttpsClient client("192.168.1.124", 5443);

	//std::string res2 = client.post("/api/upload", R"({"name":"test"})");
	//std::cout << "POST result: " << res2 << std::endl;

	ResponseRet res3 = client.put("/my/0818.txt", R"({"value":123})");
	if (res3.succeed)
		std::cout << "PUT result: " << res3.body << " " << std::endl;
	else
		std::cout << "PUT err: " << res3.errMsg << std::endl;

	ResponseRet res4 = client.get("/my/0818.txt");
	if (res4.succeed)
		std::cout << "GET result: " << res4.body << " " << std::endl;
	else
		std::cout << "GET err: " << res4.errMsg << std::endl;

	ResponseRet res5 = client.get("/my/0819.txt");
	if (res5.succeed)
		std::cout << "GET result: " << res5.body << " " << std::endl;
	else
		std::cout << "GET err: " << res5.errMsg << std::endl;

	//std::string res4 = client.del("/api/delete/1");
	//std::cout << "DELETE result: " << res4 << std::endl;
}
catch (const std::exception& ex)
{
	std::cerr << "Error: " << ex.what() << std::endl;
}
*/