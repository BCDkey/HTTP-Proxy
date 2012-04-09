#ifndef httpparser_H
#define httpparser_H

#include <string>
#include <map>
#include <vector>
#include <deque>
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

void BailOnSocketError( const char* context );
struct in_addr *atoaddr( const char* address);

typedef void (*ResponseBegin_CB)( const Response* r, void* userdata );
typedef void (*ResponseData_CB)( const Response* r, void* userdata, const unsigned char* data, int numbytes );
typedef void (*ResponseComplete_CB)( const Response* r, void* userdata );


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

class Connection
{
	friend class Response;
public:
	// ����� �� ���������
	Connection( const char* host, int port );
	~Connection();

	// ������������� ������ �������. ����� ������� ��� ��������� � pump()	
	// begincb		- ����������, ����� ������ ��������� ������
	// datacb		- ���������� ��� ������� ������ ������
	// completecb	- ����������, ����� ����� ������ ���������
	// userdata ���������� ��� �������� ��� ���� ��������.
	void setcallbacks(
		ResponseBegin_CB begincb,
		ResponseData_CB datacb,
		ResponseComplete_CB completecb,
		void* userdata );

	// ������� ����� ������������� ��������� ����������
	// ��-�� ���������� ������ �������� �������� ���� �������
	void connect();

	// ������ ����������, ������ ��� ��������� �������.
	void close();

	// ���������� ����������
	// ��� ��������� ������������� �������� ������� ������������.
	void pump();

	// �������� �� ��� �������?
	bool outstanding() const
		{ return !m_Outstanding.empty(); }

	// ��������������� ��������� ��������
	// ---------------------------
	
	// ������: GET, POST � �.�.
	// url - ������ ����� ����: � �������,  "/index.html"
	// ��������� - ������ ��� ���/��������, ��������������� �������� ���� ������
	// body � bodysize ���������� ������ ������� (��������, �������� ��� �����)
	void request( const char* method, const char* url, const char* headers[]=0,
		const unsigned char* body=0, int bodysize=0 );

	// �������������� ��������� ��������
	// ---------------------------

	// ������ ������
	// ������: GET, POST � �.�.
	// url - ������ ����� ����: � �������,  "/index.html"
	void putrequest( const char* method, const char* url );

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

private:
	enum { IDLE, REQ_STARTED, REQ_SENT } m_State;
	std::string m_Host;
	int m_Port;
	int m_Sock;
	std::vector< std::string > m_Buffer;	// ������ ��������

	std::deque< Response* > m_Outstanding;	// ������ ��� ������������� ��������
};






//-------------------------------------------------
// ����� Response
//
// ������� ������ �������.
// ------------------------------------------------


class Response
{
	friend class Connection;
public:

	// ������� ��������� (���������� 0, ���� �����������)
	const char* getheader( const char* name ) const;

	bool completed() const
		{ return m_State == COMPLETE; }


	// ������� ������-���
	int getstatus() const;

	// ������� reason-string
	const char* getreason() const;

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
	void notifyconnectionclosed();

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

	std::string m_LineBuf;		// ���������� ����� ��� ���������
	std::string m_HeaderAccum;	// ����� ��� ����������


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


#endif // httpparser_H


