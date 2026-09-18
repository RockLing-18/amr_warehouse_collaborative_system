//#define CPPHTTPLIB_OPENSSL_SUPPORT  开启则可使用https
#include "http/http_client.h"
#include "httplib/httplib.h"
#include <memory>
#include <stdexcept>
// #include "utils/LogDefine.h"

namespace amr_agent
{

HttpClient::HttpClient(const std::string& host, int port):m_host(host), m_port(port)
{}

HttpResponseRet HttpClient::get(const std::string& path)
{
	return request("GET", path, "");
}

HttpResponseRet HttpClient::post(const std::string& path, const std::string& body, const std::string& content_type)
{
	return request("POST", path, body, content_type);
}

HttpResponseRet HttpClient::put(const std::string& path, const std::string& body, const std::string& content_type)
{
	return request("PUT", path, body, content_type);
}

HttpResponseRet HttpClient::del(const std::string& path)
{
	return request("DELETE", path, "");
}

HttpResponseRet HttpClient::request(const std::string& method,
	const std::string& path,
	const std::string& body,
	const std::string& content_type)
{
	// 这里是 Client，不是 SSLClient
	httplib::Client cli(m_host, m_port);

	cli.set_connection_timeout(3, 0);
	cli.set_read_timeout(3, 0);
	cli.set_write_timeout(3, 0);

	// LOG_DEBUG("request http, ip:{},  port:{}, path:{}", m_host, m_port, path);

	httplib::Result res;

	if (method == "GET")
	{
		res = cli.Get(path.c_str());
	}
	else if (method == "POST")
	{
		res = cli.Post(path.c_str(), body, content_type.c_str());
	}
	else if (method == "PUT")
	{
		res = cli.Put(path.c_str(), body, content_type.c_str());
	}
	else if (method == "DELETE")
	{
		res = cli.Delete(path.c_str());
	}

	HttpResponseRet resRet;

	if (res)
	{
		int status = res->status;

		if (status >= 200 && status < 300)
		{
			resRet.succeed = true;

			if (status == 204)
				resRet.body = "";
			else
				resRet.body = std::move(res->body);
		}
		else
		{
			resRet.succeed = false;
			resRet.errMsg = "HTTP Error " + std::to_string(status) + ": " + res->reason;
		}
	}
	else
	{
		auto err = res.error();
		resRet.succeed = false;
		resRet.errMsg = "Request failed: " + std::string(httplib::to_string(err));
	}

	return resRet;
}

HttpResponseRet HttpClient::download(const std::string& path)
{
    httplib::Client cli(m_host, m_port);
    cli.set_connection_timeout(5,0);
    cli.set_read_timeout(30,0);

    HttpResponseRet ret;

    auto res = cli.Get(path.c_str());
    if(!res)
    {
        ret.errMsg = httplib::to_string(res.error());
        return ret;
    }

    if(res->status < 200 || res->status >=300)
    {
        ret.errMsg =  "http status:" + std::to_string(res->status);
        return ret;
    }

    ret.succeed = true;
    ret.content_type = res->get_header_value("Content-Type");
    ret.data.assign(res->body.begin(), res->body.end());
    return ret;
}

}