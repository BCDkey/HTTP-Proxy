#include "httpparser.h"

#ifndef WIN32
//	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>	// для gethostbyname()
	#include <errno.h>
#endif

#ifdef WIN32
	#include <winsock2.h>
	#define vsnprintf _vsnprintf
#endif

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <assert.h>
#include <string.h>

#include <string>
#include <vector>
#include <string>
#include <algorithm>




using namespace std;


namespace httpparser
{

#ifdef WIN32
const char* GetWinsockErrorString( int err );
#endif


// Доп. функции
//---------------------------------------------------------------------



void BailOnSocketError( const char* context )
{
#ifdef WIN32

	int e = WSAGetLastError();
	const char* msg = GetWinsockErrorString( e );
#else
	const char* msg = strerror( errno );
#endif
	throw Wobbly( "%s: %s", context, msg );
}


#ifdef WIN32

const char* GetWinsockErrorString( int err )
{
	switch( err)
	{
	case 0:					return "No error";
    case WSAEINTR:			return "Interrupted system call";
    case WSAEBADF:			return "Bad file number";
    case WSAEACCES:			return "Permission denied";
    case WSAEFAULT:			return "Bad address";
    case WSAEINVAL:			return "Invalid argument";
    case WSAEMFILE:			return "Too many open sockets";
    case WSAEWOULDBLOCK:	return "Operation would block";
    case WSAEINPROGRESS:	return "Operation now in progress";
    case WSAEALREADY:		return "Operation already in progress";
    case WSAENOTSOCK:		return "Socket operation on non-socket";
    case WSAEDESTADDRREQ:	return "Destination address required";
    case WSAEMSGSIZE:		return "Message too long";
    case WSAEPROTOTYPE:		return "Protocol wrong type for socket";
    case WSAENOPROTOOPT:	return "Bad protocol option";
	case WSAEPROTONOSUPPORT:	return "Protocol not supported";
	case WSAESOCKTNOSUPPORT:	return "Socket type not supported";
    case WSAEOPNOTSUPP:		return "Operation not supported on socket";
    case WSAEPFNOSUPPORT:	return "Protocol family not supported";
    case WSAEAFNOSUPPORT:	return "Address family not supported";
    case WSAEADDRINUSE:		return "Address already in use";
    case WSAEADDRNOTAVAIL:	return "Can't assign requested address";
    case WSAENETDOWN:		return "Network is down";
    case WSAENETUNREACH:	return "Network is unreachable";
    case WSAENETRESET:		return "Net connection reset";
    case WSAECONNABORTED:	return "Software caused connection abort";
    case WSAECONNRESET:		return "Connection reset by peer";
    case WSAENOBUFS:		return "No buffer space available";
    case WSAEISCONN:		return "Socket is already connected";
    case WSAENOTCONN:		return "Socket is not connected";
    case WSAESHUTDOWN:		return "Can't send after socket shutdown";
    case WSAETOOMANYREFS:	return "Too many references, can't splice";
    case WSAETIMEDOUT:		return "Connection timed out";
    case WSAECONNREFUSED:	return "Connection refused";
    case WSAELOOP:			return "Too many levels of symbolic links";
    case WSAENAMETOOLONG:	return "File name too long";
    case WSAEHOSTDOWN:		return "Host is down";
    case WSAEHOSTUNREACH:	return "No route to host";
    case WSAENOTEMPTY:		return "Directory not empty";
    case WSAEPROCLIM:		return "Too many processes";
    case WSAEUSERS:			return "Too many users";
    case WSAEDQUOT:			return "Disc quota exceeded";
    case WSAESTALE:			return "Stale NFS file handle";
    case WSAEREMOTE:		return "Too many levels of remote in path";
	case WSASYSNOTREADY:	return "Network system is unavailable";
	case WSAVERNOTSUPPORTED:	return "Winsock version out of range";
    case WSANOTINITIALISED:	return "WSAStartup not yet called";
    case WSAEDISCON:		return "Graceful shutdown in progress";
    case WSAHOST_NOT_FOUND:	return "Host not found";
    case WSANO_DATA:		return "No host data of that type was found";
	}

	return "unknown";
};

#endif // WIN32

//посмотри разницу структур fd_ в виндах и никсах
// true, если у соккета есть данные, ожидающие чтения
bool datawaiting( int sock )
{
	fd_set fds;
	FD_ZERO( &fds );
	FD_SET( sock, &fds );

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	int r = select( sock+1, &fds, NULL, NULL, &tv);
	if (r < 0)
		BailOnSocketError( "select" );
//переделай под асинхронный вызов
	if( FD_ISSET( sock, &fds ) )
		return true;
	else
		return false;
}


// Попытка вытянуть адрес из строки
// 0, если не вышло
struct in_addr *atoaddr( const char* address)
{
	struct hostent *host;
	static struct in_addr saddr;

	// преобразуем IP
	saddr.s_addr = inet_addr(address);
	if (saddr.s_addr != -1)
		return &saddr;

	host = gethostbyname(address);
	if( host )
		return (struct in_addr *) *host->h_addr_list;

	return 0;
}







// Класс исключений
//---------------------------------------------------------------------


Wobbly::Wobbly( const char* fmt, ... )
{
	va_list ap;
	va_start( ap,fmt);
	int n = vsnprintf( m_Message, MAXLEN, fmt, ap );
	va_end( ap );
	if(n==MAXLEN)
		m_Message[MAXLEN-1] = '\0';
}







//---------------------------------------------------------------------
//
// класс соединения
//
//---------------------------------------------------------------------
Connection::Connection( const char* host, int port ) :
	m_ResponseBeginCB(0),
	m_ResponseDataCB(0),
	m_ResponseCompleteCB(0),
	m_UserData(0),
	m_State( IDLE ),
	m_Host( host ),
	m_Port( port ),
	m_Sock(-1)
{
}

//установим колбэки
void Connection::setcallbacks(
	ResponseBegin_CB begincb,
	ResponseData_CB datacb,
	ResponseComplete_CB completecb,
	void* userdata )
{
	m_ResponseBeginCB = begincb;
	m_ResponseDataCB = datacb;
	m_ResponseCompleteCB = completecb;
	m_UserData = userdata;
}


void Connection::connect()
{
	in_addr* addr = atoaddr( m_Host.c_str() );
	if( !addr )
		throw Wobbly( "Invalid network address" );

	sockaddr_in address;
	memset( (char*)&address, 0, sizeof(address) );
	address.sin_family = AF_INET;
	address.sin_port = htons( m_Port );
	address.sin_addr.s_addr = addr->s_addr;

	m_Sock = socket( AF_INET, SOCK_STREAM, 0 );
	if( m_Sock < 0 )
		BailOnSocketError( "socket()" );

//	printf("Connecting to %s on port %d.\n",inet_ntoa(*addr), port);

	if( ::connect( m_Sock, (sockaddr const*)&address, sizeof(address) ) < 0 )
		BailOnSocketError( "connect()" );
}


void Connection::close()
{
#ifdef WIN32
	if( m_Sock >= 0 )
		::closesocket( m_Sock );
#else
	if( m_Sock >= 0 )
		::close( m_Sock );
#endif
	m_Sock = -1;

	// отмена всех незавершенных ответов
	while( !m_Outstanding.empty() )
	{
		delete m_Outstanding.front();
		m_Outstanding.pop_front();
	}
}


Connection::~Connection()
{
	close();
}

void Connection::request( const char* method,
	const char* url,
	const char* headers[],
	const unsigned char* body,
	int bodysize )
{

	bool gotcontentlength = false;	// уже есть среди заголовков?

	// проверить на наличие заголовка длины содрежимого запроса
	// TODO: проверить заголовки "Host" и "Accept-Encoding"
	// и избежать добавления их собой в putrequest()
	if( headers )
	{
		const char** h = headers;
		while( *h )
		{
			const char* name = *h++;
			const char* value = *h++;
			assert( value != 0 );	// имя без значения!

			if( 0==strcasecmp( name, "content-length" ) )
				gotcontentlength = true;
		}
	}

	putrequest( method, url );

	if( body && !gotcontentlength )
		putheader( "Content-Length", bodysize );

	if( headers )
	{
		const char** h = headers;
		while( *h )
		{
			const char* name = *h++;
			const char* value = *h++;
			putheader( name, value );
		}
	}
	endheaders();

	if( body )
		send( body, bodysize );

}




void Connection::putrequest( const char* method, const char* url )
{
	if( m_State != IDLE )
		throw Wobbly( "Request already issued" );

	m_State = REQ_STARTED;

	char req[ 512 ];
	sprintf( req, "%s %s HTTP/1.1", method, url );
	m_Buffer.push_back( req );

	putheader( "Host", m_Host.c_str() );	// для HTTP1.1

	// неизвестные энкодинги уберем
	putheader("Accept-Encoding", "identity");

	// добавить новый ответ в очередь
	Response *r = new Response( method, *this );
	m_Outstanding.push_back( r );
}


void Connection::putheader( const char* header, const char* value )
{
	if( m_State != REQ_STARTED )
		throw Wobbly( "putheader() failed" );
	m_Buffer.push_back( string(header) + ": " + string( value ) );
}

void Connection::putheader( const char* header, int numericvalue )
{
	char buf[32];
	sprintf( buf, "%d", numericvalue );
	putheader( header, buf );
}

void Connection::endheaders()
{
	if( m_State != REQ_STARTED )
		throw Wobbly( "Cannot send header" );
	m_State = IDLE;

	m_Buffer.push_back( "" );

	string msg;
	vector< string>::const_iterator it;
	for( it = m_Buffer.begin(); it != m_Buffer.end(); ++it )
		msg += (*it) + "\r\n";

	m_Buffer.clear();

//	printf( "%s", msg.c_str() );
	send( (const unsigned char*)msg.c_str(), msg.size() );
}



void Connection::send( const unsigned char* buf, int numbytes )
{
//	fwrite( buf, 1,numbytes, stdout );
	
	if( m_Sock < 0 )
		connect();

	while( numbytes > 0 )
	{
#ifdef WIN32
		int n = ::send( m_Sock, (const char*)buf, numbytes, 0 );
#else
		int n = ::send( m_Sock, buf, numbytes, 0 );
#endif
		if( n<0 )
			BailOnSocketError( "send()" );
		numbytes -= n;
		buf += n;
	}
}


void Connection::pump()
{
	if( m_Outstanding.empty() )
		return;		// нет необработанных запросов

	assert( m_Sock >0 );	// запросы есть, коннект сброшен!

	if( !datawaiting( m_Sock ) )
		return;				// запрос будет заблочен

	unsigned char buf[ 2048 ];
	int a = recv( m_Sock, (char*)buf, sizeof(buf), 0 );
	if( a<0 )
		BailOnSocketError( "recv()" );

	if( a== 0 )
	{
		// соединение закрыто

		Response* r = m_Outstanding.front();
		r->notifyconnectionclosed();
		assert( r->completed() );
		delete r;
		m_Outstanding.pop_front();

		// любые ждущие запросы будут отброшены
		close();
	}
	else
	{
		int used = 0;
		while( used < a && !m_Outstanding.empty() )
		{

			Response* r = m_Outstanding.front();
			int u = r->pump( &buf[used], a-used );

			// удалить завершенный запрос
			if( r->completed() )
			{
				delete r;
				m_Outstanding.pop_front();
			}
			used += u;
		}

		// Если очередь ответов будет пустой, будут терятся байты
		// (но сервер не должен отправлять что-либо, если еще есть что-то незавершенное
		assert( used == a );	// все байты должны быть использованы.
	}
}






// класс Response
//---------------------------------------------------------------------


Response::Response( const char* method, Connection& conn ) :
	m_Connection( conn ),
	m_State( STATUSLINE ),
	m_Method( method ),
	m_Version( 0 ),
	m_Status(0),
	m_BytesRead(0),
	m_Chunked(false),
	m_ChunkLeft(0),
	m_Length(-1),
	m_WillClose(false)
{
}


const char* Response::getheader( const char* name ) const
{
	std::string lname( name );
	std::transform( lname.begin(), lname.end(), lname.begin(), tolower );

	std::map< std::string, std::string >::const_iterator it = m_Headers.find( lname );
	if( it == m_Headers.end() )
		return 0;
	else
		return it->second.c_str();
}


int Response::getstatus() const
{
	// валидно после получения строки состояния
	assert( m_State != STATUSLINE );
	return m_Status;
}


const char* Response::getreason() const
{
	// валидно после получения строки состояния
	assert( m_State != STATUSLINE );
	return m_Reason.c_str();
}



// Соединение было закрыто
void Response::notifyconnectionclosed()
{
	if( m_State == COMPLETE )
		return;

	// EOF может быть валидным...
	if( m_State == BODY &&
		!m_Chunked &&
		m_Length == -1 )
	{
		Finish();	// готово!
	}
	else
	{
		throw Wobbly( "Connection closed unexpectedly" );
	}
}



int Response::pump( const unsigned char* data, int datasize )
{
	assert( datasize != 0 );
	int count = datasize;

	while( count > 0 && m_State != COMPLETE )
	{
		if( m_State == STATUSLINE ||
			m_State == HEADERS ||
			m_State == TRAILERS ||
			m_State == CHUNKLEN ||
			m_State == CHUNKEND )
		{
			// "собираем" строку
			while( count > 0 )
			{
				char c = (char)*data++;
				--count;
				if( c == '\n' )
				{
					// получили всю строку
					switch( m_State )
					{
						case STATUSLINE:
							ProcessStatusLine( m_LineBuf );
							break;
						case HEADERS:
							ProcessHeaderLine( m_LineBuf );
							break;
						case TRAILERS:
							ProcessTrailerLine( m_LineBuf );
							break;
						case CHUNKLEN:
							ProcessChunkLenLine( m_LineBuf );
							break;
						case CHUNKEND:
							// споймали перевод строки после тела ответа и переход к след. состоянию
							assert( m_Chunked == true );
							m_State = CHUNKLEN;
							break;
						default:
							break;
					}
					m_LineBuf.clear();
					break;		// выйти из генерации строки!
				}
				else
				{
					if( c != '\r' )		// игнорировать возврат каретки
						m_LineBuf += c;
				}
			}
		}
		else if( m_State == BODY )
		{
			int bytesused = 0;
			if( m_Chunked )
				bytesused = ProcessDataChunked( data, count );
			else
				bytesused = ProcessDataNonChunked( data, count );
			data += bytesused;
			count -= bytesused;
		}
	}

	// число использованных байт
	return datasize - count;
}



void Response::ProcessChunkLenLine( std::string const& line )
{
	// длина куска в 16-ричной в начале строки
	m_ChunkLeft = strtol( line.c_str(), NULL, 16 );
	
	if( m_ChunkLeft == 0 )
	{
		// получили все тело, проверка заголовков-трейлеров
		m_State = TRAILERS;
		m_HeaderAccum.clear();
	}
	else
	{
		m_State = BODY;
	}
}


// обработка данных при передаче частями
// возвращает число байт.
int Response::ProcessDataChunked( const unsigned char* data, int count )
{
	assert( m_Chunked );

	int n = count;
	if( n>m_ChunkLeft )
		n = m_ChunkLeft;

	// вызвать колбэк для передачи данных
	if( m_Connection.m_ResponseDataCB )
		(m_Connection.m_ResponseDataCB)( this, m_Connection.m_UserData, data, n );

	m_BytesRead += n;

	m_ChunkLeft -= n;
	assert( m_ChunkLeft >= 0);
	if( m_ChunkLeft == 0 )
	{
		// кусок завершен, пропускаем перевод строки перед трейлерами для следующего куска
		m_State = CHUNKEND;
	}
	return n;
}

// обработка данных при передаче целиком.
// возвращает число байт.
int Response::ProcessDataNonChunked( const unsigned char* data, int count )
{
	int n = count;
	if( m_Length != -1 )
	{
		// число байт известно
		int remaining = m_Length - m_BytesRead;
		if( n > remaining )
			n = remaining;
	}

	// вызов колбэка для передачи данных
	if( m_Connection.m_ResponseDataCB )
		(m_Connection.m_ResponseDataCB)( this, m_Connection.m_UserData, data, n );

	m_BytesRead += n;

	// Заканчиваем, если все готово или ждем разрыва соединения
	if( m_Length != -1 && m_BytesRead == m_Length )
		Finish();

	return n;
}


void Response::Finish()
{
	m_State = COMPLETE;

	// вызов колбэков
	if( m_Connection.m_ResponseCompleteCB )
		(m_Connection.m_ResponseCompleteCB)( this, m_Connection.m_UserData );
}


void Response::ProcessStatusLine( std::string const& line )
{
	const char* p = line.c_str();

	// пропустить любые предшествующие пробелы
	while( *p && *p == ' ' )
		++p;

	// версия протокола
	while( *p && *p != ' ' )
		m_VersionString += *p++;
	while( *p && *p == ' ' )
		++p;

	// статус-код
	std::string status;
	while( *p && *p != ' ' )
		status += *p++;
	while( *p && *p == ' ' )
		++p;

	// остальное - строка причины
	while( *p )
		m_Reason += *p++;

	m_Status = atoi( status.c_str() );
	if( m_Status < 100 || m_Status > 999 )
		throw Wobbly( "BadStatusLine (%s)", line.c_str() );

/*
	printf( "version: '%s'\n", m_VersionString.c_str() );
	printf( "status: '%d'\n", m_Status );
	printf( "reason: '%s'\n", m_Reason.c_str() );
*/

	if( m_VersionString == "HTTP:/1.0" )
		m_Version = 10;
	else if( 0==m_VersionString.compare( 0,7,"HTTP/1." ) )
		m_Version = 11;
	else
		throw Wobbly( "UnknownProtocol (%s)", m_VersionString.c_str() );
	// HTTP/0.9 не поддерживает

	
	// теперь идут заголовки
	m_State = HEADERS;
	m_HeaderAccum.clear();
}


// обработка данных заголовка
void Response::FlushHeader()
{
	if( m_HeaderAccum.empty() )
		return;	// не нужна

	const char* p = m_HeaderAccum.c_str();

	std::string header;
	std::string value;
	while( *p && *p != ':' )
		header += tolower( *p++ );

	// пропуск ':'
	if( *p )
		++p;

	// пропуск пробелов
	while( *p && (*p ==' ' || *p=='\t') )
		++p;

	value = p; // остальная часть строки - значение

	m_Headers[ header ] = value;
//	printf("header: ['%s': '%s']\n", header.c_str(), value.c_str() );	

	m_HeaderAccum.clear();
}


void Response::ProcessHeaderLine( std::string const& line )
{
	const char* p = line.c_str();
	if( line.empty() )
	{
		FlushHeader();
		// конец заголовков

		// код 100 игнорируется
		if( m_Status == CONTINUE )
			m_State = STATUSLINE;	// сброс парсинга и ожидание новой строки состояния
		else
			BeginBody();			// обработка тела ответа
		return;
	}

	if( isspace(*p) )
	{
		// строка продолжения - добавить к предшествующим данным
		++p;
		while( *p && isspace( *p ) )
			++p;

		m_HeaderAccum += ' ';
		m_HeaderAccum += p;
	}
	else
	{
		// взять новый заголовок
		FlushHeader();
		m_HeaderAccum = p;
	}
}


void Response::ProcessTrailerLine( std::string const& line )
{
	// Сделать: обработка доп. заголовков	
	if( line.empty() )
		Finish();

	// просто игнорируем доп. заголовки
}



// Сперва проверка информации, полученной из заголовков, затем - переход к разбору тела ответа
void Response::BeginBody()
{

	m_Chunked = false;
	m_Length = -1;	// неизвестна
	m_WillClose = false;

	// используется кодировка?
	const char* trenc = getheader( "transfer-encoding" );
	if( trenc && 0==strcasecmp( trenc, "chunked") )
	{
		m_Chunked = true;
		m_ChunkLeft = -1;	// неизвестно
	}

	m_WillClose = CheckClose();

	// задана длина?
	const char* contentlen = getheader( "content-length" );
	if( contentlen && !m_Chunked )
	{
		m_Length = atoi( contentlen );
	}

	// проверка состояний, при которых ожидается нулевой объем данных
	if( m_Status == NO_CONTENT ||
		m_Status == NOT_MODIFIED ||
		( m_Status >= 100 && m_Status < 200 ) ||		// 1xx коды без тела ответа
		m_Method == "HEAD" )
	{
		m_Length = 0;
	}


	// если не используется частичная передача и длина не была определена,
	// убедится, что соединение будет разорвано.
	if( !m_WillClose && !m_Chunked && m_Length == -1 )
		m_WillClose = true;



	// вызвов пользовательских колбэков, если есть
	if( m_Connection.m_ResponseBeginCB )
		(m_Connection.m_ResponseBeginCB)( this, m_Connection.m_UserData );

/*
	printf("---------BeginBody()--------\n");
	printf("Length: %d\n", m_Length );
	printf("WillClose: %d\n", (int)m_WillClose );
	printf("Chunked: %d\n", (int)m_Chunked );
	printf("ChunkLeft: %d\n", (int)m_ChunkLeft );
	printf("----------------------------\n");
*/
	// чтение данных тела
	if( m_Chunked )
		m_State = CHUNKLEN;
	else
		m_State = BODY;
}


// true, если сервер сам разорвет соединение
bool Response::CheckClose()
{
	if( m_Version == 11 )
	{
		// HTTP1.1
		// соединение открыто, даже если "connection: close" определено.
		const char* conn = getheader( "connection" );
		if( conn && 0==strcasecmp( conn, "close" ) )
			return true;
		else
			return false;
	}

	// более старые версии HTTP
	// keep-alive заголовок указывает на постоянное соединение 
	if( getheader( "keep-alive" ) )
		return false;	
	return true;
}



}	// конец пространства имен