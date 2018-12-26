/* ssl_client.c
 *
 * Copyright (c) 2000 Sean Walton and Macmillan Publishers.  Use may be in
 * whole or in part in accordance to the General Public License (GPL).
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

/*****************************************************************************/
/*** ssl_client.c                                                          ***/
/***                                                                       ***/
/*** Demonstrate an SSL client.                                            ***/
/*****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "common.h"

#define FAIL    -1

/*---------------------------------------------------------------------*/
/*--- OpenConnection - create socket and connect to server.         ---*/
/*---------------------------------------------------------------------*/
int OpenConnection(const char *hostname, int port)
{   int sd;
    struct hostent *host;
    struct sockaddr_in addr;

    if ( (host = gethostbyname(hostname)) == NULL )
    {
        perror(hostname);
        abort();
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);
    if ( connect(sd, &addr, sizeof(addr)) != 0 )
    {
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}

/*---------------------------------------------------------------------*/
/*--- InitCTX - initialize the SSL engine.                          ---*/
/*---------------------------------------------------------------------*/
SSL_CTX* InitCTX(void)
{   SSL_METHOD *method;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();       /* Load cryptos, et.al. */
    SSL_load_error_strings();           /* Bring in and register error messages */
    method = TLSv1_2_client_method();       /* Create new client-method instance */
    ctx = SSL_CTX_new(method);          /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

/*---------------------------------------------------------------------*/
/*--- ShowCerts - print out the certificates.                       ---*/
/*---------------------------------------------------------------------*/
void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);   /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);                         /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);                         /* free the malloc'ed string */
        X509_free(cert);                    /* free the malloc'ed certificate copy */
    }
    else
        printf("No certificates.\n");
}

/*---------------------------------------------------------------------*/
/*--- main - create SSL context and connect                         ---*/
/*---------------------------------------------------------------------*/
//apt-get update
//apt-get install openssl
//apt-get install libssl-dev
//https://github.com/mrwicks/miscellaneous/blob/master/tls_1.2_example/client.c
//https://www.yahoo.com/news/weather/united-states/maryland/frederick-12519814
//gcc -Wall -o ssl_client ssl_client.c -L/usr/lib -lssl -lcrypto
bool frederick(float *out_temperature) {   
    SSL_CTX *ctx;
    int server;
    SSL *ssl;
    char buf[2048];
    int bytes;
    char *hostname, *portnum;

    int temperature = -999;
    
    hostname="www.yahoo.com";
    portnum="443";
    char *tag = "<span class=\"Va(t)\" data-reactid=\"33\">";

    ctx = InitCTX();
    server = OpenConnection(hostname, atoi(portnum));
    ssl = SSL_new(ctx);                     /* create new SSL connection state */
    SSL_set_fd(ssl, server);                /* attach the socket descriptor */
    if ( SSL_connect(ssl) == FAIL )         /* perform the connection */
        ERR_print_errors_fp(stderr);
    else
    {   char *msg = "GET /news/weather/united-states/maryland/frederick-2372860 HTTP/1.1\r\nHost: www.yahoo.com\r\nAccept: text/plain, text/html, text/*\r\nUser-Agent: Mozilla/5.0 (iPad; CPU OS 6_0_1 like Mac OS X) AppleWebKit/536.26 (KHTML, like Gecko) Mobile/10A523\r\nAccept-Language: en-us\r\n\r\n";

        //printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
        //ShowCerts(ssl);                               /* get any certs */
        SSL_write(ssl, msg, strlen(msg));           /* encrypt & send message */
        //this is chunked-encoding
        bytes = SSL_read(ssl, buf, sizeof(buf));    /* get reply & decrypt */
        while (bytes > 0) {
            buf[bytes] = '\0';
            /*
            <div class="now Fz(8em)--sm Fz(10em) Lh(0.9em) Fw(100) My(2px) Trsdu(.3s)" data-reactid="32">
                <span class="Va(t)" data-reactid="33">40</span>
                <span class="Va(t) Fz(.7em) Lh(1em)" data-reactid="34">°</span>
                <div class="unit-control D(ib)" data-reactid="35">
                    <button data-unit="imperial" aria-label="°Fahrenheit" class="unit Tt(c) Fz(.2em) Fw(200) O(n) P(6px) Va(t) D(b) Lh(1em) Tsh($temperature-text-shadow) M(a) C(#fff)" data-reactid="36">F</button>
                    <button data-unit="metric" aria-label="°Celsius" class="unit Tt(c) Fz(.2em) Fw(200) O(n) P(6px) Va(t) 
            */ 
            char *ptr1 = strstr(buf, tag);
            if (ptr1) {
                ptr1 += strlen (tag);
                char *ptr2 = strstr(ptr1, "<");
                *ptr2='\0';
                temperature = atoi(ptr1);
                
                break;
            }
            bytes = SSL_read(ssl, buf, sizeof(buf));    /* get reply & decrypt */
        }
        SSL_free(ssl);                              /* release connection state */
    }
    close(server);                                  /* close socket */
    SSL_CTX_free(ctx);                              /* release context */
    
    *out_temperature = temperature;
    return temperature > -999;
}
