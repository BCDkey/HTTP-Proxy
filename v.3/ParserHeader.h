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

//диаграммка из httplib.py
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

//сделай реализацию для стороны клиент-прокси!!!!
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

// коды статсуа HTTP
enum {
	// 1xx информационные
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,
	PROCESSING = 102,

	// 2xx успешное завершение
	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,
	MULTI_STATUS = 207,
	IM_USED = 226,

	// 3xx перенаправление
	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305,
	TEMPORARY_REDIRECT = 307,
	
	// 4xx ошибки клиента
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

	// 5xx ошибки сервера
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	INSUFFICIENT_STORAGE = 507,
	NOT_EXTENDED = 510,
};



// Класс исключений

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



// класс соединения
// Держит соединение соккета, отправляет запросы и обрабатывает ответы
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
	// сразу не коннектит
	Connection( const char* host, int port, const char* version );
	~Connection();

	// обслуживающие ответы колбэки. Будут вызваны при обращении к pump()	
	// begincb		- вызывается, когда пришел заголовок ответа
	// datacb		- вызывается для разбора данных ответа
	// completecb	- вызывается, когда ответ пришел полностью
	// userdata передается как параметр для всех колбэков.
	void setcallbacks(
		ResponseBegin_CB begincbm,
		ResponseData_CB datacbm,
		ResponseComplete_CB completecbm,
		void* userdatam,
		RequestBegin_CB	begincbr,
		RequestData_CB datacbr,
		RequestComplete_CB completecbr,
		void* userdatar);

	// Запросы будут автоматически создавать соединение
	// Из-за блокировки иногда придется вызывать явно заранее
	void connect();

	// разрыв соединения, убирая все ожидающие запросы.
	void close();

	// Обновление соединения
	// Для обработки невыполненных запросов вызывай периодически.
	void pump();
	void createresponse(const Response* r);
	void createrequest(const Request* r);
	// остались ли еще запросы?
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
	// высокоуровневый интерфейс запросов
	// ---------------------------
	
	// методы: GET, POST и т.д.
	// url - только часть пути: к примеру,  "/index.html"
	// заголовки - наборы пар имя/значение, заканчивающихся символом нуля строки
	// body и bodysize определяют данные запроса (например, значения для формы)
	void request( const char* method, const char* url, const char* headers[]=0,
		const unsigned char* body=0, int bodysize=0 );
	void response( const char* version, const char* reason, const char* status,	const char* headers[]=0,
		const unsigned char* body=0, int bodysize=0 );
	// низкоуровневый интерфейс запросов
	// ---------------------------

	// начать запрос
	// методы: GET, POST и т.д.
	// url - только часть пути: к примеру,  "/index.html"
	void putrequest( const char* method, const char* url );
	void putresponse( const char* version, const char* reason, const char* status);
	// добавление заголовка запросу
	void putheader( const char* header, const char* value );
	void putheader( const char* header, int numericvalue );

	// отправка запроса после добавления последнего заголовка.
	void endheaders();

	// отправка данных, если есть.
	// вызывается после endheaders()
	void send( const unsigned char* buf, int numbytes );

protected:
	// необходимые поля для класса Response

	// колбэки
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
	std::vector< std::string > r_Buffer;	// набора ответов
	std::vector< std::string > m_Buffer;	// наборы запросов
	bool gothost;
	enum { NONE, DONE } resp_State;
	enum { RESP, REQ } response_or_request;
	bool first_time_connection;
	bool gotacceptencoding;
	std::deque< Request* > r_Outstanding;
	std::deque< Response* > m_Outstanding;	// ответы для невыполненных запросов
};

//-------------------------------------------------
// класс Request
//
// Парсинг данных запросов.
// ------------------------------------------------
class Request
{
	friend class Connection;
	friend class ProxyServer;
public:
	// извлечь заголовок (возвращает 0, если отсутствует)
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

	// достать данные для обработки.
	// возвращает число использованных байт.
	// Всегда 0, если ответ обработан.
	int pump( const unsigned char* data, int datasize );

	// предупредить о разорванном соединении
	void notifyconnectionclosed();

private:
	enum {
		METHODURL,		// начинаем со строки состояния.
		HEADERS,		// читаем хэдеры
		BODY,			// ожидаем данные тела ответа (все или часть)
		CHUNKLEN,		// ожидание индикатора длины полученных данных (in hex)
		CHUNKEND,		// ожидаем пустую строку после получения части данных (дальше могут идти доп. заголовки)
		TRAILERS,		// чтение доп. заголовков (трейлеры) после тела ответа.
		COMPLETE,		// ответ разобран
	} r_State;

	Connection& r_Connection;	// для доступа к указателям колбэков
	std::string r_Method;		// методы: GET, POST и т.д.
	std::string r_URL;
	std::string r_VersionString;

	std::vector<std::string> r_Headers;

	int		r_BytesRead;		// число считанных байт тела ответа
	bool	r_Chunked;			// разбит ли ответ на части?
	int		r_ChunkLeft;		// оставшееся число байт в текущей части данных
	int		r_Length;			// -1, если неизвестно
	bool	r_WillClose;		// оборвать ли соединение после прихода ответа?
	int		r_Version;
	std::string r_LineBuf;		// сохранение строк для состояний
	std::string r_HeaderAccum;	// буфер для заголовков
	
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
// класс Response
//
// Парсинг данных ответов.
// ------------------------------------------------


class Response
{
	friend class Connection;
	friend class ProxyServer;
public:

	void clearall();
	// извлечь заголовок (возвращает 0, если отсутствует)
	const char* getheader( const char* name ) const;

	bool completed() const
		{ return m_State == COMPLETE; }

	bool bodydone() const
	{ return m_State == BODY||CHUNKLEN; }

	// извлечь статус-код
	int getstatus() const;

	// извлечь reason-string
	const char* getreason() const;

	Connection *getconnection() const
	{
		return &(this->m_Connection);
	}
	// true, если соединение разрывается после получения ответа.
	bool willclose() const
		{ return m_WillClose; }
protected:
	// используется объектами Connection

	// только объекты класса Connection создают Response.
	Response( const char* method, Connection& conn );

	// достать данные для обработки.
	// возвращает число использованных байт.
	// Всегда 0, если ответ обработан.
	int pump( const unsigned char* data, int datasize );

	// предупредить о разорванном соединении
	bool notifyconnectionclosed();

private:
	enum {
		STATUSLINE,		// начинаем со строки состояния.
		HEADERS,		// читаем хэдеры
		BODY,			// ожидаем данные тела ответа (все или часть)
		CHUNKLEN,		// ожидание индикатора длины полученных данных (in hex)
		CHUNKEND,		// ожидаем пустую строку после получения части данных (дальше могут идти доп. заголовки)
		TRAILERS,		// чтение доп. заголовков (трейлеры) после тела ответа.
		COMPLETE,		// ответ разобран
	} m_State;

	Connection& m_Connection;	// для доступа к указателям колбэков
	std::string m_Method;		// методы: GET, POST и т.д.

	// строка состояния
	std::string	m_VersionString;	// версия протокола
	int	m_Version;			// 10: HTTP/1.0    11: HTTP/1.x (где x>=1)
	int m_Status;			// Статус-код
	std::string m_Reason;	// reason-string

	// карта пар заголовок/значение
	std::map<std::string,std::string> m_Headers;

	int		m_BytesRead;		// число считанных байт тела ответа
	bool	m_Chunked;			// разбит ли ответ на части?
	int		m_ChunkLeft;		// оставшееся число байт в текущей части данных
	int		m_Length;			// -1, если неизвестно
	bool	m_WillClose;		// оборвать ли соединение после прихода ответа?
	bool	cache_allowed;		// разрешить кеширование
	std::string m_LineBuf;		// сохранение строк для состояний
	std::string m_HeaderAccum;	// буфер для заголовков
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


