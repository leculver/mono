thisdir = class/System.Net.Http.WebRequest
SUBDIRS = 
include ../../build/rules.make

LIBRARY = System.Net.Http.WebRequest.dll

LIB_REFS = System.Net.Http System System.Core
KEYFILE = ../msfinal.pub
LIB_MCS_FLAGS =

TEST_MCS_FLAGS = -d:WEBREQUEST_HANDLER
TEST_LIB_REFS = System System.Core System.Net.Http System.Net.Http.WebRequest

ifdef SOCKETSHTTPHANDLER
DOTNET_URI = 1
PROFILE_TEST_HARNESS_EXCLUDES = LegacyHttpClient,
else
socketshandlertests =
PROFILE_TEST_HARNESS_EXCLUDES = SocketsHttpHandler,
endif

include ../../build/library.make
