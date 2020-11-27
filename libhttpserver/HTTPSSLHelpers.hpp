#pragma once
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>

namespace HTTP::SSL{

	inline void _InitSSL(){
		::SSL_load_error_strings();
		::OpenSSL_add_ssl_algorithms();
	}

	inline void _CleanupSSL(){
		EVP_cleanup();
	}


	struct SSL_CTX_Deleter{
		void operator()(SSL_CTX* ssl_ctx){
			::SSL_CTX_free(ssl_ctx);
			HTTP::SSL::_CleanupSSL();
		}
	};

	struct SSL_Deleter{
		void operator()(::SSL* ssl){
			::SSL_shutdown(ssl);
			::SSL_free(ssl);
		}
	};

	inline void _SSLConfigureContext(SSL_CTX* SSLContext, const char* ssl_cert_file_path, const char* ssl_private_key_path){
		SSL_CTX_set_ecdh_auto(SSLContext, 1);
		if(::SSL_CTX_use_certificate_file(SSLContext, ssl_cert_file_path, SSL_FILETYPE_PEM) <= 0){
			::ERR_print_errors_fp(stderr);
			::exit(EXIT_FAILURE);
		}
		if(::SSL_CTX_use_PrivateKey_file(SSLContext, ssl_private_key_path, SSL_FILETYPE_PEM) <= 0){
			::ERR_print_errors_fp(stderr);
			::exit(EXIT_FAILURE);
		}
	}

	inline std::unique_ptr<SSL_CTX, HTTP::SSL::SSL_CTX_Deleter> _SSLCreateContext(){
		const ::SSL_METHOD* SSLMethod = ::SSLv23_server_method();
		std::unique_ptr<::SSL_CTX, HTTP::SSL::SSL_CTX_Deleter> SSLContext{::SSL_CTX_new(SSLMethod)};
		if(!SSLContext.get()){
			std::perror("SSL Context creation error");
			ERR_print_errors_fp(stderr);
			::exit(EXIT_FAILURE);
		}
		return SSLContext;
	}

	std::unique_ptr<::SSL_CTX, HTTP::SSL::SSL_CTX_Deleter> HTTPConfigSSLContext(const std::string& SSL_cert_path, const std::string& SSL_private_key_path){
		HTTP::SSL::_InitSSL();
		std::unique_ptr<::SSL_CTX, HTTP::SSL::SSL_CTX_Deleter> SSLContext{_SSLCreateContext()};
		HTTP::SSL::_SSLConfigureContext(SSLContext.get(), SSL_cert_path.c_str(), SSL_private_key_path.c_str());
		return SSLContext;
	}

	inline std::unique_ptr<::SSL, HTTP::SSL::SSL_Deleter> SSLConnectionAccept(SSL_CTX* ssl_ctx, int client_net_fd){
		std::unique_ptr<::SSL, HTTP::SSL::SSL_Deleter> SSLConnHandler{SSL_new(ssl_ctx)};
		SSL_set_fd(SSLConnHandler.get(), client_net_fd);
		int ssl_acc_ret = ::SSL_accept(SSLConnHandler.get());
		if(ssl_acc_ret < 0){
			::ERR_print_errors_fp(stderr);
			::exit(EXIT_FAILURE);
		}
		return SSLConnHandler;
	}

	// inline void SSL_read_date(SSL* ssl, char* client_read_buffer, std::size_t client_read_buffer_size){ // ERROR: variable has incomplete type void
        //
	// }


} // end namespace HTTP::SSL
