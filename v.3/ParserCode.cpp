#include "ParserHeader.h"

#ifndef WIN32
//	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	//#include <afxwin.h>
	//#include <afxcmn.h>
	#include <netdb.h>	// для gethostbyname()
	#include <errno.h>
#endif

/*#ifdef WIN32
	#include <winsock2.h>
	#define vsnprintf _vsnprintf
#endif*/
//#include <afxwin.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <assert.h>
#include <iostream>
#include <string.h>
//#include <windows.h>
#include <string>
#include <vector>
#include <string>
#include <algorithm>
#include <iphlpapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment (lib, "iphlpapi")

using namespace std;


namespace httpparser
{

//кэш: первая строка - url+пару заголовков, вторая строка - ответ сервера, третяя строка - имя файла на проксе
std::map< std::map< std::vector<std::string>, std::string>, std::string > cache;
HANDLE general_mutex[6];
DWORD retcode = 3;
//число подключенных пользователей
long number_of_connections;

#ifdef WIN32
const char* GetWinsockErrorString( int err );
#endif

class Connection;
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
	WaitForSingleObject(general_mutex[0], INFINITE);
	cout<<context<<msg<<endl;
	ReleaseMutex(general_mutex[0]);
//	throw Wobbly( "%s: %s", context, msg );
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
	{
		BailOnSocketError( "select" );
	}
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

void OnBeginm( const httpparser::Response* r, void* userdata )
{
	//WaitForSingleObject(general_mutex[0], INFINITE);
	int got;
	int timeout = 0;
	r->getconnection()->createresponse(r);
	int len = r->getconnection()->get_resp_done().size();
	char *buf = (char*)malloc(len*sizeof(char));
	strcpy(buf, r->getconnection()->get_resp_done().c_str());
	WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<"headers_resp_parsed"<<endl;
			//close();
	ReleaseMutex(general_mutex[0]);
try_again_beginm:
	got = ::send(r->getconnection()->getsock_2(), (const char*)buf, len, 0);
	if( got < 0 )
	{
		if(timeout==5)
		{
			BailOnSocketError(" send() ");
			r->getconnection()->close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		Sleep(200);
		timeout++;
		goto try_again_beginm;
	}
	//ReleaseMutex(general_mutex[0]);
	//printf( "BEGIN (%d %s)\n", r->getstatus(), r->getreason() );
	//count = 0;
}

void OnDatam( const httpparser::Response* r, void* userdata, const unsigned char* data, int n )
{
	//WaitForSingleObject(general_mutex[1], INFINITE);
	int got;
	int timeout=0;
	WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<"data_resp_parsing"<<endl;
			//close();
	ReleaseMutex(general_mutex[0]);
	//const char* buf = reinterpret_cast <const char*> (data);
try_again_datam:
	got = ::send(r->getconnection()->getsock_2(), (const char*)data, n, 0);
	if( got < 0 )
	{
		if(timeout==5)
		{
			BailOnSocketError(" send() ");
			r->getconnection()->close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		Sleep(200);
		timeout++;
		goto try_again_datam;
	}
	//fwrite( data,1,n, pFile );
	//count += n;
	//if(n!=2048)
	//	count+=n;
	//ReleaseMutex(general_mutex[1]);
}

void OnCompletem( const httpparser::Response* r, void* userdata )
{
	//printf( "COMPLETE (%d bytes)\n", count );
}

void OnBeginr( const httpparser::Request* r, void* userdata )
{
	//WaitForSingleObject(general_mutex[2],INFINITE);
	int got;
	int timeout=0;
	WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<"headers_req_parsed"<<endl;
			//close();
	ReleaseMutex(general_mutex[0]);
	//const Request *req = r;
	r->getconnection()->createrequest(r);
	r->getconnection()->connect();
	int len = r->getconnection()->get_req_done().size();
	char *buf = (char*)malloc(len*sizeof(char));
	strcpy(buf, r->getconnection()->get_req_done().c_str());
	SOCKET sock = r->getconnection()->getsock_1();
try_again_beginr:
	got = ::send(r->getconnection()->getsock_1(), (const char*)buf, len, 0);
	if( got < 0 )
	{
		if(timeout==5)
		{
			BailOnSocketError(" send() ");
			r->getconnection()->close();
			//r->clearall();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		Sleep(200);
		timeout++;
		goto try_again_beginr;
	}
	//Response *resp = new Response //(r->r_Method, r->getconnection()
	//printf( "BEGIN (%d %s)\n", r->getstatus(), r->getreason() );
	//count = 0;
	//ReleaseMutex(general_mutex[2]);
}

void OnDatar( const httpparser::Request* r, void* userdata, const unsigned char* data, int n )
{
	//WaitForSingleObject(general_mutex[3],INFINITE);
	int timeout=0;
	int got;
	WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<"data_req_parsing"<<endl;
			//close();
	ReleaseMutex(general_mutex[0]);
	const char* buf = reinterpret_cast <const char*> (data);
try_again_datar:
	got = ::send(r->getconnection()->getsock_1(), buf, n, 0);
	if( got < 0 )
	{
		if(timeout==5)
		{
			BailOnSocketError(" send() ");
			//r->clearall();
			r->getconnection()->close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		Sleep(200);
		timeout++;
		goto try_again_datar;
	}
	//fwrite( data,1,n, pFile );
	//count += n;
	//if(n!=2048)
	//	count+=n;
	//ReleaseMutex(general_mutex[3]);
}

void OnCompleter( const httpparser::Request* r, void* userdata )
{
	//printf( "COMPLETE (%d bytes)\n", count );
}

//Класс сервера
//---------------------------------------------------------------------

ProxyServer::ProxyServer(int onport) :
port (onport),
number_of_connections (100)
{
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(onport);
	local_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	mysocket = socket( AF_INET, SOCK_STREAM, 0 );
	if( mysocket < 0 )
		BailOnSocketError( "socket()" );
	for each(HANDLE ha in general_mutex)
	{
		ha = INVALID_HANDLE_VALUE;
	}
}

void ProxyServer::whatismyIP()
{
	//using std::cout;
	//using std::cerr;
	using std::endl;

	char* buf;
	PIP_ADAPTER_INFO pAdaptersInfo;
	PIP_ADDR_STRING pAddr;
	DWORD dwSize = 0;

	if(GetAdaptersInfo(NULL, &dwSize) != ERROR_BUFFER_OVERFLOW)
	{
		cerr << "GetAdaptersInfo fail" << endl;
		return;
	}
	buf = new char[dwSize];
	if (!buf) return;
	pAdaptersInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buf);
	if (GetAdaptersInfo (pAdaptersInfo, &dwSize) == ERROR_SUCCESS){
		while (pAdaptersInfo){
			pAddr = &pAdaptersInfo->IpAddressList;
			while (pAddr){
				cout <<"\nAddress: " << pAddr->IpAddress.String << "/" << 
					pAddr->IpMask.String << endl;
				pAddr = pAddr->Next;
			}
			pAdaptersInfo = pAdaptersInfo->Next;
		}
	}
	delete[] buf;
}

void ProxyServer::bind_this_port()
{
	//bind( mysocket,(LPSOCKADDR)&local_addr, sizeof(struct sockaddr) );
	if (bind(mysocket, (sockaddr *)&local_addr, sizeof(struct sockaddr)))
     {
         // Ошибка
         printf("Error bind %d\n", WSAGetLastError());
         closesocket(mysocket); // закрываем сокет!
         WSACleanup();         
     }
	if (listen(mysocket, number_of_connections))
     {
         // Ошибка
         printf("Error listen %d\n", WSAGetLastError());
         closesocket(mysocket);
         WSACleanup();         
     }	
}

ProxyServer::~ProxyServer()
{
	close();
}

void ProxyServer::keepconnections()
{
	int temp_sock;
	for each (HANDLE h in this->HANDLES)
		h = INVALID_HANDLE_VALUE;
	sockaddr_in client_addr;
	int client_addr_size = sizeof(client_addr);
	for(int i = 0; i<6 ; i++)
	{
		general_mutex[i] = CreateMutexA(NULL, false, NULL);
	}
	while(1)
	{
		if(!datawaiting(this->mysocket))
			continue;
		while(temp_sock = ::accept(mysocket, (sockaddr *)&client_addr, 
			&client_addr_size))
		{
			httpparser::number_of_connections++;
			WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<httpparser::number_of_connections<<endl;
			//close();
			ReleaseMutex(general_mutex[0]);
			Connection *conn = new Connection("",80,"");
			conn->setsock_2(temp_sock);
			conn->setport_2(ntohs(client_addr.sin_port));
			//DWORD thID;
			for (int i = 0;i < 100; i++)
				if( GetExitCodeThread(HANDLES[i], &retcode) == 0)
				{
				/*	if(HANDLES[i]!=INVALID_HANDLE_VALUE)
					{
						CloseHandle(HANDLES[i]);
						HANDLES[i] = INVALID_HANDLE_VALUE;
					}*/
					HANDLES[i] = CreateThread(NULL, NULL, NewConnection, (LPVOID)conn, NULL, NULL);	
					break;
				}
		}
	}
}

DWORD WINAPI NewConnection(LPVOID connection)
{
	Connection *conn = (Connection*)connection;
	conn->setcallbacks(OnBeginm, OnDatam, OnCompletem, 0, OnBeginr, OnDatar, OnCompleter, 0);
	while(!conn->getrout().empty() || conn->first() == true || !conn->getmout().empty())
	{
		conn->pump();
	}
	return 0;
}
void ProxyServer::close()
{
#ifdef WIN32
	if( mysocket >= 0 )
		::closesocket( mysocket );	
#else
	if( mysocket >= 0 )
		::close( mysocket );	
#endif
	mysocket = -1;
	for each(HANDLE ha in general_mutex)
	{
		if(httpparser::general_mutex!=INVALID_HANDLE_VALUE)
		CloseHandle(httpparser::general_mutex);
	}

	httpparser::cache.clear();	
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
Connection::Connection( const char* host, int port, const char* version ) :
	m_ResponseBeginCB(0),
	m_ResponseDataCB(0),
	m_ResponseCompleteCB(0),
	r_RequestBeginCB(0),
	r_RequestDataCB(0),
	r_RequestCompleteCB(0),
	r_UserData(0),
	m_UserData(0),
	m_State( IDLE ),
	first_time_connection (true),
	resp_State ( NONE),
	m_Version ( version ),
	m_Host( host ),
	m_Port( port ),
	m_Sock(-1),
	m2_Sock(-1),
	m2_Port( 0 ),
	gotacceptencoding ( false ),
	gothost ( false ),
	response_or_request( REQ )
{
}

//установим колбэки
void Connection::setcallbacks(
	ResponseBegin_CB begincbm,
	ResponseData_CB datacbm,
	ResponseComplete_CB completecbm,
	void* userdatam,
	RequestBegin_CB begincbr,
	RequestData_CB datacbr,
	RequestComplete_CB completecbr,
	void* userdatar)
{
	m_ResponseBeginCB = begincbm;
	m_ResponseDataCB = datacbm;
	m_ResponseCompleteCB = completecbm;
	m_UserData = userdatam;
	r_RequestBeginCB = begincbr;
	r_RequestDataCB = datacbr;
	r_RequestCompleteCB = completecbr;
	r_UserData = userdatar;
	/*if(this->first_time_connection)
	{
		Request *req = new Request(*this);
		this->r_Outstanding.push_back(req);
	}*/
}

//ИСПОЛЬЗУЙ ЕГО ДЛЯ КОННЕКТА, КОГДА БУДЕТ ГОТОВ ЗАПРОС В ПАМПЕ
void Connection::connect()
{
	in_addr* addr = atoaddr( m_Host.c_str() );
	if( !addr  )
	{
		WaitForSingleObject(general_mutex[0], INFINITE);
		cout<<"Invalid network address"<<endl;
		close();
		ReleaseMutex(general_mutex[0]);
		httpparser::number_of_connections--;
		ExitThread(retcode);
		//throw Wobbly( "Invalid network address" );
	}
	sockaddr_in address;
	memset( (char*)&address, 0, sizeof(address) );
	address.sin_family = AF_INET;
	address.sin_port = htons( m_Port );
	address.sin_addr.s_addr = addr->s_addr;

	m_Sock = socket( AF_INET, SOCK_STREAM, 0 );
	if( m_Sock < 0 )
	{
		BailOnSocketError( "socket()" );
		close();
		httpparser::number_of_connections--;
		ExitThread(retcode);
	}
//	printf("Connecting to %s on port %d.\n",inet_ntoa(*addr), port);

	if( ::connect( m_Sock, (sockaddr const*)&address, sizeof(address) ) < 0 )
	{
		BailOnSocketError( "connect()" );
		close();
		httpparser::number_of_connections--;
		ExitThread(retcode);
	}
}


void Connection::close()
{
#ifdef WIN32
	if( m_Sock >= 0 )
		::closesocket( m_Sock );
	if( m2_Sock >= 0 )
		::closesocket( m_Sock );
#else
	if( m_Sock >= 0 )
		::close( m_Sock );
	if( m2_Sock >= 0 )
		::close( m2_Sock );
#endif
	m_Sock = -1;
	m2_Sock = -1;
	// отмена всех незавершенных ответов
	while( !m_Outstanding.empty() )
	{
		delete m_Outstanding.front();
		m_Outstanding.pop_front();
	}
	while( !r_Outstanding.empty() )
	{
		delete r_Outstanding.front();
		r_Outstanding.pop_front();
	}
	this->m_Buffer.clear();
	this->r_Buffer.clear();
	this->m_Version.clear();
	this->req_done.clear();
	this->resp_done.clear();
	this->m_Version.clear();
		if(m_UserData)
	free(this->m_UserData);
		if(r_UserData)
	free(this->r_UserData);
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

			if( 0==strcmpi( name, "content-length" ) )
				gotcontentlength = true;
			if( 0==strcmpi( name, "accept-encoding" ) )
				this->gotacceptencoding = true;
			if( 0==strcmpi( name, "host" ) )
				this->gothost = true;
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

void Connection::response( const char* version, const char* reason, const char* status,	const char* headers[],
	const unsigned char* body, int bodysize )
{
	putresponse( version, reason, status );	

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

void Connection::putresponse(const char* version, const char* reason, const char* status)
{
	if( m_State != IDLE )
		throw Wobbly( "Response already issued" );
	m_State = REQ_STARTED;
	char req[ 4096 ];
	sprintf( req, "%s %s %s", version, reason, status );
	r_Buffer.push_back( req );
	/*Request *r = new Request(*this);
	r_Outstanding.push_back(r);*/
}
void Connection::putrequest( const char* method, const char* url )
{
	if( m_State != IDLE )
		throw Wobbly( "Request already issued" );

	m_State = REQ_STARTED;

	char req[ 4096 ];
	sprintf( req, "%s %s %s", method, url, m_Version.c_str() );
	m_Buffer.push_back( req );
	if(!gothost)
	putheader( "Host", m_Host.c_str() );	// для HTTP1.1
	if(!gotacceptencoding)
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
	if(response_or_request==REQ)
	m_Buffer.push_back( string(header) + ": " + string( value ) );
	else
		r_Buffer.push_back( string(header) + ": " + string( value ) );
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
	
	string msg;

	if( response_or_request == REQ )
	{
		m_Buffer.push_back( "" );
		vector< string>::const_iterator it;
		for( it = m_Buffer.begin(); it != m_Buffer.end(); ++it )
			msg += (*it) + "\r\n";

		m_Buffer.clear();
		this->req_done = msg;
	}
	else
	{
		r_Buffer.push_back( "" );
		vector< string>::const_iterator it;
		for( it = r_Buffer.begin(); it != r_Buffer.end(); ++it )
			msg += (*it) + "\r\n";

		r_Buffer.clear();
		this->resp_done = msg;
	}	
//	printf( "%s", msg.c_str() );
	//send( (const unsigned char*)msg.c_str(), msg.size() );
}


//ВРЯД ЛИ БУДЕШЬ ВООБЩЕ ЕГО ЮЗАТЬ, ДЕЛАЙ ОТПРАВКУ ДАННЫХ И ЗАПРОСА НАПРЯМУЮ В СОККЕТ В КОЛБЭКАХ
void Connection::send( const unsigned char* buf, int numbytes )
{
//	fwrite( buf, 1,numbytes, stdout );
	int *sock = &m2_Sock;
	if( response_or_request == REQ )
		sock = &m_Sock;
	if( *sock < 0 )
		connect();

	while( numbytes > 0 )
	{
#ifdef WIN32
		int n = ::send( *sock, (const char*)buf, numbytes, 0 );
#else
		int n = ::send( *sock, buf, numbytes, 0 );
#endif
		if( n<0 )
		{
			BailOnSocketError( "send()" );
			close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		numbytes -= n;
		buf += n;
	}
}

void Connection::createresponse(const Response* r)
{
	this->response_or_request= RESP;
	if( resp_State != NONE )
		return;

	resp_State = DONE;
	char dest[4];
	string msg = r->m_VersionString;
	msg+= " ";
	string status = itoa(r->m_Status, dest, 10);
	msg+= status;
	msg+= " ";
	msg+= r->m_Reason;
	msg+= "\r\n";
	std::map<std::string, std::string>::const_iterator it;
	for( it = r->m_Headers.begin(); it != r->m_Headers.end(); ++it )
		msg += it->first + ": " + it->second + "\r\n";
	msg+="\r\n";
	this->resp_done = msg;
}
//ДОДЕЛАЙ ЭТУ ХЕРНЮ И СДЕЛАЙ ШТУЧКУ С ХОСТОМ
void Connection::createrequest(const Request* r)
{
	this->response_or_request = REQ;
	std::vector<std::string>::const_iterator it;
	bool ishost = false;
	for(it = r->r_Headers.begin(); it != r->r_Headers.end();++it)
	{
		if(ishost) { this->m_Host += it->data(); ishost = false;}
		if( 0==strcmpi( it->data(), "Host" ) )
		{ishost=true; this->gothost=true;}
		if( 0==strcmpi( it->data(), "Accept-Encoding" ) ) this->gotacceptencoding = true;
	}
	this->m_Version = r->r_VersionString;
	this->putrequest( r->r_Method.c_str(), r->r_URL.c_str() );
	for( it = r->r_Headers.begin(); it != r->r_Headers.end();++it)
	{
		const char *buf = it->data();
		it++;
		if(buf=="Content-Length")
		{
			int bodysize = atoi(it->data());
			this->putheader(buf, bodysize);
			continue;
		}		
		this->putheader(buf, it->data());
	}
	endheaders();
}
void Connection::pump()
{
	int timeout = 0;
	bool trythis = false;
	/*WaitForSingleObject(general_mutex[0], INFINITE);
			cout<<"got in pump"<<endl;
			//close();
	ReleaseMutex(general_mutex[0]);*/
	unsigned char buf_m[ 4096 ];
	if(first_time_connection == true)
	{
		Request *r = new Request(*this);
		//r->setConnection(this);
		this->r_Outstanding.push_back( r );
		first_time_connection = false;
	}
	if( r_Outstanding.empty() )
		return;
	assert( m2_Sock >0 );
	if( !datawaiting(m2_Sock ) )
	{
		if(m_Outstanding.empty() )
		{
			/*close();
			httpparser::number_of_connections--;
			ExitThread(retcode);*/
			return;
		}
		//return;
		else goto response_processing;
	}
	unsigned char buf_r[ 4096 ];
try_again_req:
	int b = recv (m2_Sock, (char*)buf_r, sizeof(buf_r), 0);
	if(b<0)
	{
		Sleep(200);		
		if(timeout==5)
		{
			BailOnSocketError("recv()");
			close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		timeout++;
		goto try_again_req;
	}
	if( b == 0 ) //отсутствие данных в соккете
	{
		// соединение закрыто

		Request* r = r_Outstanding.front();
		r->notifyconnectionclosed();
		assert( r->completed() );
		/*delete r;
		this->r_Outstanding.pop_front();
		// любые ждущие запросы будут отброшены
		close();
		ExitThread(retcode);*/
	}
	else
	{
		int used_r = 0;
		while( used_r < b && !r_Outstanding.empty() )
		{

			Request* r = r_Outstanding.front();
			int u_r = r->pump( &buf_r[used_r], b-used_r );
			// удалить завершенный запрос
			if( r->completed() )
			{
				delete r;
				r_Outstanding.pop_front();
				first_time_connection = true;
				//Request *new_r = new Request(*this);
				//r_Outstanding.push_back(new_r);
			}
			used_r += u_r;
		}
		if( used_r != b )
		{
				//assert( used_r == b );
				httpparser::number_of_connections--;
				ExitThread(retcode);
		}
	}
response_processing:
	timeout=0;
	if( m_Outstanding.empty() )
		return;		// нет необработанных запросов

	assert( m_Sock >0 );	// запросы есть, коннект сброшен!

	if( !datawaiting( m_Sock ) )
	{
		//if(!datawaiting( m2_Sock))
		/*{
			while(!m_Outstanding.empty())
			{
				Response *resp_del = m_Outstanding.front();
				resp_del->clearall();
				m_Outstanding.pop_front();
			}
			while(!r_Outstanding.empty())
			{
				Request *req_del = r_Outstanding.front();
				req_del->clearall();
				r_Outstanding.pop_front();
			}
			/*free(&buf_r);
			free(&buf_m);*/
			/*close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}*/
		//else
		return;				// recv будет заблочен
	}
	//int timeout = 0;
try_again:
	int a = recv( m_Sock, (char*)buf_m, sizeof(buf_m), 0 );
	if( a<0 )
	{
		Sleep(200);
		if(timeout == 5)
		{
			BailOnSocketError( "recv()" );
			while(!m_Outstanding.empty())
			{
				Response *resp_del = m_Outstanding.front();
				resp_del->clearall();
				m_Outstanding.pop_front();
			}
			while(!r_Outstanding.empty())
			{
				Request *req_del = r_Outstanding.front();
				req_del->clearall();
				r_Outstanding.pop_front();
			}
			/*free(&buf_r);
			free(&buf_m);*/
			close();
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
		timeout++;
		goto try_again;
	}
	if( a== 0 ) //отсутствие данных в соккете
	{
		// соединение закрыто
		
		Response* m = m_Outstanding.front();
		trythis = m->notifyconnectionclosed();
		if(trythis)
		{
			this->connect();
			timeout=0;
			trythis=false;
			goto try_again;
		}
		assert( m->completed() );
		delete m;
		m_Outstanding.pop_front();

		// любые ждущие запросы будут отброшены
		//close();
	}
	else
	{
		int used_m = 0;
		while( used_m < a && !m_Outstanding.empty() )
		{

			Response* m = m_Outstanding.front();
			int u_m = m->pump( &buf_m[used_m], a-used_m );
			/*if (m->bodydone() )
			{
				this->createresponse(m);
			}*/
			// удалить завершенный запрос
			if( m->completed() )
			{
				delete m;
				m_Outstanding.pop_front();
				close();
			}
			used_m += u_m;
		}		
		// Если очередь ответов будет пустой, будут теряться байты
		// (но сервер не должен отправлять что-либо, если еще есть что-то незавершенное)
		if(used_m != a)
		{
			//assert( used_m == a );	// все байты должны быть использованы.
			httpparser::number_of_connections--;
			ExitThread(retcode);
			//httpparser::number_of_connections--;
		}
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
	extremetry (true),
	cache_allowed(false),
	m_ChunkLeft(0),
	m_Length(-1),
	m_WillClose(false)
{
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

const char* Response::getheader( const char* name ) const
{
	std::string lname( name );
	//std::transform( lname.begin(), lname.end(), lname.begin(),);

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
void Response::clearall()
{
	this->m_HeaderAccum.clear();
	this->m_Headers.clear();
	this->m_LineBuf.clear();
	this->m_Method.clear();
	this->m_Reason.clear();
	this->m_VersionString.clear();
}
bool Response::notifyconnectionclosed()
{
	if( m_State == COMPLETE )
		return false;

	// EOF может быть валидным...
	if( m_State == BODY &&
		!m_Chunked &&
		m_Length == -1 )
	{
		Finish();	// готово!
	}
	else
	{
		if(extremetry==false)
		{
			WaitForSingleObject(general_mutex[0], INFINITE);
				cout<<"Connection closed unexpectedly"<<endl;
				this->getconnection()->close();
				//this->clearall();
			ReleaseMutex(general_mutex[0]);
			httpparser::number_of_connections--;
			ExitThread(retcode);
		}
			extremetry=false;
			return true;
		//throw Wobbly( "Connection closed unexpectedly" );
	}
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
		header += *p++ ;

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
	const char* trenc = getheader( "Transfer-Encoding" );
	if( trenc && 0==strcmpi( trenc, "chunked") )
	{
		m_Chunked = true;
		m_ChunkLeft = -1;	// неизвестно
	}

	m_WillClose = CheckClose();

	// задана длина?
	const char* contentlen = getheader( "Content-Length" );
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
		if( conn && 0==strcmpi( conn, "close" ) )
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

Request::Request( Connection& conn ) :
	r_Connection( conn ),
	r_State( METHODURL ),
	r_BytesRead(0),
	r_Chunked(false),
	r_ChunkLeft(0),
	r_Length(-1),
	r_WillClose(false),
	//r_Method("NONE"),
	//r_URL("NONE"),
	r_Version(0)
	//r_VersionString("NONE")
{
}

int Request::pump( const unsigned char* data, int datasize )
{
	assert( datasize != 0 );
	int count = datasize;

	while( count > 0 && r_State != COMPLETE )
	{
		if( r_State == METHODURL ||
			r_State == HEADERS ||
			r_State == TRAILERS ||
			r_State == CHUNKLEN ||
			r_State == CHUNKEND )
		{
			// "собираем" строку
			while( count > 0 )
			{
				char c = (char)*data++;
				--count;
				if( c == '\n' )
				{
					// получили всю строку
					switch( r_State )
					{
					case METHODURL:
						ProcessMethodUrlLine( r_LineBuf );
						break;
					case HEADERS:
						ProcessHeaderLine( r_LineBuf );
						break;
					case TRAILERS:
						ProcessTrailerLine( r_LineBuf );
						break;
					case CHUNKLEN:
						ProcessChunkLenLine( r_LineBuf );
						break;
					case CHUNKEND:
						// споймали перевод строки после тела ответа и переход к след. состоянию
						assert( r_Chunked == true );
						r_State = CHUNKLEN;
						break;
					default:
						break;
					}
					r_LineBuf.clear();
					break;		// выйти из генерации строки!
				}
				else
				{
					if( c != '\r' )		// игнорировать возврат каретки
						r_LineBuf += c;
				}
			}
		}
		else if( r_State == BODY )
		{
			int bytesused = 0;
			if( r_Chunked )
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

const char* Request::getheader( const char* name ) const
{
	std::string lname( name );
	std::transform( lname.begin(), lname.end(), lname.begin(), tolower );
	std::vector< std::string >::const_iterator it = std::find(r_Headers.begin(), r_Headers.end(), lname );
	if( it == r_Headers.end() )
		return 0;
	else
		return it->data();
}

void Request::clearall()
{
	this->r_HeaderAccum.clear();
	this->r_Headers.clear();
	this->r_LineBuf.clear();
	this->r_Method.clear();
	//this->r_Reason.clear();
	this->r_VersionString.clear();
}

/*int Request::getstatus() const
{
	// валидно после получения строки состояния
	assert( r_State != METHODURL );
	return r_Status;
}
*/

/*const char* Request::getreason() const
{
	// валидно после получения строки состояния
	assert( m_State != STATUSLINE );
	return m_Reason.c_str();
}*/


// Соединение было закрыто
void Request::notifyconnectionclosed()
{
	if( r_State == COMPLETE )
		return;

	// EOF может быть валидным...
	if( r_State == BODY &&
		!r_Chunked &&
		r_Length == -1 )
	{
		Finish();	// готово!
	}
	else
	{
			WaitForSingleObject(general_mutex[0], INFINITE);
				cout<<"Connection closed"<<endl;
				this->getconnection()->close();
				//this->clearall();
			ReleaseMutex(general_mutex[0]);
			httpparser::number_of_connections--;
			ExitThread(retcode);
	}
}

void Request::ProcessChunkLenLine( std::string const& line )
{
	// длина куска в 16-ричной в начале строки
	r_ChunkLeft = strtol( line.c_str(), NULL, 16 );
	
	if( r_ChunkLeft == 0 )
	{
		// получили все тело, проверка заголовков-трейлеров
		r_State = TRAILERS;
		r_HeaderAccum.clear();
	}
	else
	{
		r_State = BODY;
	}
}


// обработка данных при передаче частями
// возвращает число байт.
int Request::ProcessDataChunked( const unsigned char* data, int count )
{
	assert( r_Chunked );

	int n = count;
	if( n>r_ChunkLeft )
		n = r_ChunkLeft;

	// вызвать колбэк для передачи данных
	if( r_Connection.r_RequestDataCB )
		(r_Connection.r_RequestDataCB)( this, r_Connection.r_UserData, data, n );

	r_BytesRead += n;

	r_ChunkLeft -= n;
	assert( r_ChunkLeft >= 0);
	if( r_ChunkLeft == 0 )
	{
		// кусок завершен, пропускаем перевод строки перед трейлерами для следующего куска
		r_State = CHUNKEND;
	}
	return n;
}

// обработка данных при передаче целиком.
// возвращает число байт.
int Request::ProcessDataNonChunked( const unsigned char* data, int count )
{
	int n = count;
	if( r_Length != -1 )
	{
		// число байт известно
		int remaining = r_Length - r_BytesRead;
		if( n > remaining )
			n = remaining;
	}

	// вызов колбэка для передачи данных
	if( r_Connection.r_RequestDataCB )
		(r_Connection.r_RequestDataCB)( this, r_Connection.r_UserData, data, n );

	r_BytesRead += n;

	// Заканчиваем, если все готово или ждем разрыва соединения
	if( r_Length != -1 && r_BytesRead == r_Length )
		Finish();

	return n;
}


void Request::Finish()
{
	r_State = COMPLETE;

	// вызов колбэков
	if( r_Connection.r_RequestCompleteCB )
		(r_Connection.r_RequestCompleteCB)( this, r_Connection.r_UserData );
}

void Request::ProcessMethodUrlLine( std::string const& line )
{
	const char* p = line.c_str();

	// пропустить любые предшествующие пробелы
	while( *p && *p == ' ' )
		++p;

	// метод
	while( *p && *p != ' ' )
		r_Method += *p++;
	while( *p && *p == ' ' )
		++p;

	// URL
	while( *p && *p != ' ' )
		r_URL += *p++;
	while( *p && *p == ' ' )
		++p;

	// остальное - версия
	while( *p )
		r_VersionString += *p++;

/*
	printf( "version: '%s'\n", m_VersionString.c_str() );
	printf( "status: '%d'\n", m_Status );
	printf( "reason: '%s'\n", m_Reason.c_str() );
*/

	if( r_VersionString == "HTTP:/1.0" )
		r_Version = 10;
	else if( 0==r_VersionString.compare( 0,7,"HTTP/1." ) )
		r_Version = 11;
	else
		throw Wobbly( "UnknownProtocol (%s)", r_VersionString.c_str() );
	// HTTP/0.9 не поддерживает

	
	// теперь идут заголовки
	r_State = HEADERS;
	r_HeaderAccum.clear();
}

// обработка данных заголовка
void Request::FlushHeader()
{
	if( r_HeaderAccum.empty() )
		return;	// не нужна

	const char* p = r_HeaderAccum.c_str();

	std::string header;
	std::string value;
	while( *p && *p != ':' )
		header +=  *p++;

	// пропуск ':'
	if( *p )
		++p;

	// пропуск пробелов
	while( *p && (*p ==' ' || *p=='\t') )
		++p;

	value = p; // остальная часть строки - значение

	r_Headers.push_back(header);
	r_Headers.push_back(value);
//	printf("header: ['%s': '%s']\n", header.c_str(), value.c_str() );	

	r_HeaderAccum.clear();
}


void Request::ProcessHeaderLine( std::string const& line )
{
	const char* p = line.c_str();
	if( line.empty() )
	{
		FlushHeader();
		BeginBody();			// обработка тела ответа
		return;
	}

	if( isspace(*p) )
	{
		// строка продолжения - добавить к предшествующим данным
		++p;
		while( *p && isspace( *p ) )
			++p;
		r_HeaderAccum += ' ';
		r_HeaderAccum += p;
	}
	else
	{
		// взять новый заголовок
		FlushHeader();
		r_HeaderAccum = p;
	}
}


void Request::ProcessTrailerLine( std::string const& line )
{
	// Сделать: обработка доп. заголовков	
	if( line.empty() )
		Finish();	
	// просто игнорируем доп. заголовки
}



// Сперва проверка информации, полученной из заголовков, затем - переход к разбору тела ответа
void Request::BeginBody()
{

	r_Chunked = false;
	r_Length = -1;	// неизвестна
	r_WillClose = false;

	// используется кодировка?
	const char* trenc = getheader( "Transfer-Encoding" );
	if( trenc && 0==strcmpi( trenc, "chunked") )
	{
		r_Chunked = true;
		r_ChunkLeft = -1;	// неизвестно
	}

	r_WillClose = CheckClose();

	// задана длина?
	const char* contentlen = getheader( "Content-Length" );
	if( contentlen && !r_Chunked )
	{
		this->r_Length = atoi( contentlen );
	}
	if(this->r_Method != "POST" && this->r_Method != "PATCH" && this->r_Method != "PUT")
		this->r_Length = 0;

	// если не используется частичная передача и длина не была определена,
	// убедится, что соединение будет разорвано.
	if( !r_WillClose && !r_Chunked && r_Length == -1 )
		r_WillClose = true;



	// вызвов пользовательских колбэков, если есть
	if( r_Connection.r_RequestBeginCB )
		(r_Connection.r_RequestBeginCB)( this, r_Connection.r_UserData );

/*
	printf("---------BeginBody()--------\n");
	printf("Length: %d\n", m_Length );
	printf("WillClose: %d\n", (int)m_WillClose );
	printf("Chunked: %d\n", (int)m_Chunked );
	printf("ChunkLeft: %d\n", (int)m_ChunkLeft );
	printf("----------------------------\n");
*/
	// чтение данных тела
	if( r_Chunked )
		r_State = CHUNKLEN;
	else
		r_State = BODY;
	if(r_Length == 0)
		Finish(); 
}


// true, если сервер сам разорвет соединение
bool Request::CheckClose()
{
	if( r_Version == 11 )
	{
		// HTTP1.1
		// соединение открыто, даже если "connection: close" определено.
		const char* conn = getheader( "connection" );
		if( conn && 0==strcmpi( conn, "close" ) )
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