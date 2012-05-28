#include "ParserHeader.h"
//#include "happyhttp.h"
using namespace httpparser;

int main(int argc, char* argv[])
{
	#ifdef WIN32
    WSAData wsaData;
    int code = WSAStartup(MAKEWORD(1, 1), &wsaData);
	if( code != 0 )
	{
		printf("shite. %d\n",code);
		return 0;
	}
#endif //WIN32
	int port;
	printf ("Input the port to bing with:\n");
	scanf ("%d", &port);
	ProxyServer *prx = new ProxyServer(port);
	try
	{
		prx->whatismyIP();
		prx->bind_this_port();
		prx->keepconnections();
		//Test2();
		//Test3();
	}

	catch( httpparser::Wobbly& e )
	{
		printf("Exception:\n%s\n", e.what() );
		prx->close();
	}
	
#ifdef WIN32
    WSACleanup();
#endif // WIN32

	return 0;
}
