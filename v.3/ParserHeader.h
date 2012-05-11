#ifndef ParserHeader_H
#define ParserHeader_H

#include <string>
#include <map>
#include <vector>
#include <deque>
#ifdef WIN32
#include <winsock2.h>
#define vsnprintf _vsnprintf
#endif
#define _AFXDLL
/*#ifdef WIN32
	#include <winsock2.h>
	#define vsnprintf _vsnprintf
#endif*/
//#include <afxwin.h>
#include <Windows.h>

//���������� �� httplib.py
/*(null)
|
| HTTPConnection()
v
Idle
|
| putrequest()
v
Request-started
|
| ( putheader() )*  endheaders()
v
Request-sent
|
| response = getresponse()
v
Unread-response   [Response-headers-read]
|\____________________
|                     |
| response.read()     | putrequest()
v                     v
Idle                  Req-started-unread-response
				 ______/|
			   /        |
response.read()|        | ( putheader() )*  endheaders()
               v        v
	Request-started     Req-sent-unread-response
						|
						| response.read()
						v
						Request-sent
*/

//������ ���������� ��� ������� ������-������!!!!
struct in_addr;

namespace httpparser
{
class Response;
class Request;
//void whatismyIP();
void BailOnSocketError( const char* context );
void want_to_print(const char* str);
DWORD WINAPI NewConnection(LPVOID connection);
struct in_addr *atoaddr( const char* address);

typedef void (*ResponseBegin_CB)( const Response* r, void* userdata );
typedef void (*ResponseData_CB)( const Response* r, void* userdata, const unsigned char* data, int numbytes );
typedef void (*ResponseComplete_CB)( const Response* r, void* userdata );
typedef void (*RequestBegin_CB)( const Request* r, void* userdata );
typedef void (*RequestData_CB)( const Request* r, void* userdata, const unsigned char* data, int numbytes );
typedef void (*RequestComplete_CB)( const Request* r, void* userdata );

// ���� ������� HTTP
enum {
	// 1xx ��������������
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,
	PROCESSING = 102,

	// 2xx �������� ����������
	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,
	MULTI_STATUS = 207,
	IM_USED = 226,

	// 3xx ���������������
	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305,
	TEMPORARY_REDIRECT = 307,
	
	// 4xx ������ �������
	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	PROXY_AUTHENTICATION_REQUIRED = 407,
	REQUEST_TIMEOUT = 408,
	CONFLICT = 409,
	GONE = 410,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	REQUEST_ENTITY_TOO_LARGE = 413,
	REQUEST_URI_TOO_LONG = 414,
	UNSUPPORTED_MEDIA_TYPE = 415,
	REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,
	UNPROCESSABLE_ENTITY = 422,
	LOCKED = 423,
	FAILED_DEPENDENCY = 424,
	UPGRADE_REQUIRED = 426,

	// 5xx ������ �������
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	INSUFFICIENT_STORAGE = 507,
	NOT_EXTENDED = 510,
};



// ����� ����������

class Wobbly
{
public:
	Wobbly( const char* fmt, ... );
	const char* what() const
		{ return m_Message; }
protected:
	enum { MAXLEN=256 };
	char m_Message[ MAXLEN ];
};



// ����� ����������
// ������ ���������� �������, ���������� ������� � ������������ ������
// ------------------------------------------------
class Connection;
class ProxyServer
{
	friend class Connection;
public:
	ProxyServer(int port);
	~ProxyServer();
	void whatismyIP();
	void bind_this_port();
	void close();
	void keepconnections();
	HANDLE HANDLES[100];
private:
	int port;
	SOCKET mysocket;
	sockaddr_in local_addr;
	int number_of_connections;	
};
class Connection
{
	friend class Response;
	friend class Request;
public:
	// ����� �� ���������
	Connection( const char* host, int port, const char* version );
	~Connection();

	// ������������� ������ �������. ����� ������� ��� ��������� � pump()	
	// begincb		- ����������, ����� ������ ��������� ������
	// datacb		- ���������� ��� ������� ������ ������
	// completecb	- ����������, ����� ����� ������ ���������
	// userdata ���������� ��� �������� ��� ���� ��������.
	void setcallbacks(
		ResponseBegin_CB begincbm,
		ResponseData_CB datacbm,
		ResponseComplete_CB completecbm,
		void* userdatam,
		RequestBegin_CB	begincbr,
		RequestData_CB datacbr,
		RequestComplete_CB completecbr,
		void* userdatar);

	// ������� ����� ������������� ��������� ����������
	// ��-�� ���������� ������ �������� �������� ���� �������
	void connect();

	// ������ ����������, ������ ��� ��������� �������.
	void close();

	// ���������� ����������
	// ��� ��������� ������������� �������� ������� ������������.
	void pump();
	void createresponse(const Response* r);
	void createrequest(const Request* r);
	// �������� �� ��� �������?
	bool moutstanding() const
		{ return !m_Outstanding.empty(); }
	bool routstaning() const
	{
		return !r_Outstanding.empty();
	}
	std::deque< Request* > getrout()
	{
		return r_Outstanding;
	}
	std::deque< Response* > getmout()
	{
		return m_Outstanding;
	}
	void setsock_1(int sock)
	{
		m_Sock = sock;
	}
	void setsock_2(int sock)
	{
		m2_Sock = sock;
	}
	int getsock_1()
	{
		return m_Sock;
	}
	int getsock_2()
	{
		return m2_Sock;
	}
	std::string get_resp_done() const
	{
		return resp_done;
	}
	std::string get_req_done() const
	{
		return req_done;
	}
	bool first() const
	{
		return first_time_connection;
	}
	void setport_1(int port)
	{
		m_Port = port;
	}
	void setport_2(int port)
	{
		m2_Port = port;
	}
	// ��������������� ��������� ��������
	// ---------------------------
	
	// ������: GET, POST � �.�.
	// url - ������ ����� ����: � �������,  "/index.html"
	// ��������� - ������ ��� ���/��������, ��������������� �������� ���� ������
	// body � bodysize ���������� ������ ������� (��������, �������� ��� �����)
	void request( const char* method, const char* url, const char* headers[]=0,
		const unsigned char* body=0, int bodysize=0 );
	void response( const char* version, const char* reason, const char* status,	const char* headers[]=0,
		const unsigned char* body=0, int bodysize=0 );
	// �������������� ��������� ��������
	// ---------------------------

	// ������ ������
	// ������: GET, POST � �.�.
	// url - ������ ����� ����: � �������,  "/index.html"
	void putrequest( const char* method, const char* url );
	void putresponse( const char* version, const char* reason, const char* status);
	// ���������� ��������� �������
	void putheader( const char* header, const char* value );
	void putheader( const char* header, int numericvalue );

	// �������� ������� ����� ���������� ���������� ���������.
	void endheaders();

	// �������� ������, ���� ����.
	// ���������� ����� endheaders()
	void send( const unsigned char* buf, int numbytes );

protected:
	// ����������� ���� ��� ������ Response

	// �������
	ResponseBegin_CB	m_ResponseBeginCB;
	ResponseData_CB		m_ResponseDataCB;
	ResponseComplete_CB	m_ResponseCompleteCB;
	void*				m_UserData;
	RequestBegin_CB		r_RequestBeginCB;
	RequestData_CB		r_RequestDataCB;
	RequestComplete_CB	r_RequestCompleteCB;
	void*				r_UserData;

private:
	enum { IDLE, REQ_STARTED, REQ_SENT } m_State;
	std::string m_Host;
	int m_Port;
	int m2_Port;
	int m_Sock;
	int m2_Sock;
	std::string m_Version;
	std::string resp_done;
	std::string req_done;
	std::vector< std::string > r_Buffer;	// ������ �������
	std::vector< std::string > m_Buffer;	// ������ ��������
	bool gothost;
	enum { NONE, DONE } resp_State;
	enum { RESP, REQ } response_or_request;
	bool first_time_connection;
	bool gotacceptencoding;
	std::deque< Request* > r_Outstanding;
	std::deque< Response* > m_Outstanding;	// ������ ��� ������������� ��������
};

//-------------------------------------------------
// ����� Request
//
// ������� ������ ��������.
// ------------------------------------------------
class Request
{
	friend class Connection;
	friend class ProxyServer;
public:
	// ������� ��������� (���������� 0, ���� �����������)
	const char* getheader( const char* name ) const;

	void setConnection(Connection* conn)
	{
		this->r_Connection = *conn;
	}
	void clearall();
	bool completed() const
	{ return r_State == COMPLETE; }

	bool bodydone() const
	{ return r_State == BODY||CHUNKLEN; }

	std::string getmethod() const
	{
		return r_Method;
	}
	bool willclose() const
	{ return r_WillClose; }
	
	Connection *getconnection() const
	{
		return &(this->r_Connection);
	}

	//int getstatus() const;
protected:
	Request( Connection& conn );

	// ������� ������ ��� ���������.
	// ���������� ����� �������������� ����.
	// ������ 0, ���� ����� ���������.
	int pump( const unsigned char* data, int datasize );

	// ������������ � ����������� ����������
	void notifyconnectionclosed();

private:
	enum {
		METHODURL,		// �������� �� ������ ���������.
		HEADERS,		// ������ ������
		BODY,			// ������� ������ ���� ������ (��� ��� �����)
		CHUNKLEN,		// �������� ���������� ����� ���������� ������ (in hex)
		CHUNKEND,		// ������� ������ ������ ����� ��������� ����� ������ (������ ����� ���� ���. ���������)
		TRAILERS,		// ������ ���. ���������� (��������) ����� ���� ������.
		COMPLETE,		// ����� ��������
	} r_State;

	Connection& r_Connection;	// ��� ������� � ���������� ��������
	std::string r_Method;		// ������: GET, POST � �.�.
	std::string r_URL;
	std::string r_VersionString;

	std::vector<std::string> r_Headers;

	int		r_BytesRead;		// ����� ��������� ���� ���� ������
	bool	r_Chunked;			// ������ �� ����� �� �����?
	int		r_ChunkLeft;		// ���������� ����� ���� � ������� ����� ������
	int		r_Length;			// -1, ���� ����������
	bool	r_WillClose;		// �������� �� ���������� ����� ������� ������?
	int		r_Version;
	std::string r_LineBuf;		// ���������� ����� ��� ���������
	std::string r_HeaderAccum;	// ����� ��� ����������
	
	void FlushHeader();
	void ProcessMethodUrlLine( std::string const& line );
	void ProcessHeaderLine( std::string const& line );
	void ProcessTrailerLine( std::string const& line );
	void ProcessChunkLenLine( std::string const& line );

	int ProcessDataChunked( const unsigned char* data, int count );
	int ProcessDataNonChunked( const unsigned char* data, int count );

	void BeginBody();
	bool CheckClose();
	void Finish();
};
//-------------------------------------------------
// ����� Response
//
// ������� ������ �������.
// ------------------------------------------------


class Response
{
	friend class Connection;
	friend class ProxyServer;
public:

	void clearall();
	// ������� ��������� (���������� 0, ���� �����������)
	const char* getheader( const char* name ) const;

	bool completed() const
		{ return m_State == COMPLETE; }

	bool bodydone() const
	{ return m_State == BODY||CHUNKLEN; }

	// ������� ������-���
	int getstatus() const;

	// ������� reason-string
	const char* getreason() const;

	Connection *getconnection() const
	{
		return &(this->m_Connection);
	}
	// true, ���� ���������� ����������� ����� ��������� ������.
	bool willclose() const
		{ return m_WillClose; }
protected:
	// ������������ ��������� Connection

	// ������ ������� ������ Connection ������� Response.
	Response( const char* method, Connection& conn );

	// ������� ������ ��� ���������.
	// ���������� ����� �������������� ����.
	// ������ 0, ���� ����� ���������.
	int pump( const unsigned char* data, int datasize );

	// ������������ � ����������� ����������
	bool notifyconnectionclosed();

private:
	enum {
		STATUSLINE,		// �������� �� ������ ���������.
		HEADERS,		// ������ ������
		BODY,			// ������� ������ ���� ������ (��� ��� �����)
		CHUNKLEN,		// �������� ���������� ����� ���������� ������ (in hex)
		CHUNKEND,		// ������� ������ ������ ����� ��������� ����� ������ (������ ����� ���� ���. ���������)
		TRAILERS,		// ������ ���. ���������� (��������) ����� ���� ������.
		COMPLETE,		// ����� ��������
	} m_State;

	Connection& m_Connection;	// ��� ������� � ���������� ��������
	std::string m_Method;		// ������: GET, POST � �.�.

	// ������ ���������
	std::string	m_VersionString;	// ������ ���������
	int	m_Version;			// 10: HTTP/1.0    11: HTTP/1.x (��� x>=1)
	int m_Status;			// ������-���
	std::string m_Reason;	// reason-string

	// ����� ��� ���������/��������
	std::map<std::string,std::string> m_Headers;

	int		m_BytesRead;		// ����� ��������� ���� ���� ������
	bool	m_Chunked;			// ������ �� ����� �� �����?
	int		m_ChunkLeft;		// ���������� ����� ���� � ������� ����� ������
	int		m_Length;			// -1, ���� ����������
	bool	m_WillClose;		// �������� �� ���������� ����� ������� ������?
	bool	cache_allowed;		// ��������� �����������
	std::string m_LineBuf;		// ���������� ����� ��� ���������
	std::string m_HeaderAccum;	// ����� ��� ����������
	bool	extremetry;

	void FlushHeader();
	void ProcessStatusLine( std::string const& line );
	void ProcessHeaderLine( std::string const& line );
	void ProcessTrailerLine( std::string const& line );
	void ProcessChunkLenLine( std::string const& line );

	int ProcessDataChunked( const unsigned char* data, int count );
	int ProcessDataNonChunked( const unsigned char* data, int count );

	void BeginBody();
	bool CheckClose();
	void Finish();
};



}


#endif // ParserHeader_H


