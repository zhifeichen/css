# for linux compile css4.0.
#
CSS_APP_NAME = css4.0
PWD = $(shell pwd)
ROOT_DIR = $(PWD)/../../
# binary dir
BIN_DIR = $(ROOT_DIR)bin/
# *.so dir
LIB_PATH = $(BIN_DIR)
# include dir
INCLUDE_DIR = $(ROOT_DIR)include/
# src dir
SRC_DIR = $(ROOT_DIR)src/
SRC_FILES = $(foreach dir,$(SRC_DIR),$(wildcard $(dir)*.c))

EXE = $(BIN_DIR)$(CSS_APP_NAME).boot
EXE_TEST = $(BIN_DIR)$(CSS_APP_NAME).test.boot
# --------------------------------------------------------

# libuv && gyp.
LIBUV_NAME_APP = uv
LIBUV_GIT_ADDR = git@192.168.203.211:libuv.git
LIBUV_NAME = lib$(LIBUV_NAME_APP)
LIBUV_SOURCE_DIR = $(DEPS_ROOT_DIRS)$(LIBUV_NAME)/
LIBUV_SOURCE_BUILD_DIR = $(LIBUV_SOURCE_DIR)build/
LIBUV_INCLUDE_SOURCE_DIR = $(LIBUV_SOURCE_DIR)include/
LIBUV_INCLUDE_DIR = $(INCLUDE_DIR)$(LIBUV_NAME_APP)/

GYP_NAME_APP = gyp
GYP_GIT_ADDR = git@192.168.203.211:gyp.git
GYP_SOURCE_DEPS_DIR = $(DEPS_ROOT_DIRS)$(GYP_NAME_APP)/
# --------------------------------------------------------

# mysql c API
LIBMYSQL_NAME_APP = mysql
LIBMYSQL_GIT_ADDR = git@192.168.203.211:libmysql.git
LIBMYSQL_NAME = lib$(LIBMYSQL_NAME_APP)
LIBMYSQL_SOURCE_DIR = $(DEPS_ROOT_DIRS)$(LIBMYSQL_NAME)/
LIBMYSQL_BUILD_DIR = $(LIBMYSQL_SOURCE_DIR)build/
LIBMYSQL_INCLUDE_SOURCE_DIR = $(LIBMYSQL_SOURCE_DIR)include/
LIBMYSQL_INCLUDE_DIR = $(INCLUDE_DIR)$(LIBMYSQL_NAME_APP)/
#--------------------------------------------------------

#mxml
LIBMXML_NAME_APP = mxml
LIBMXML_GIT_ADDR = git@192.168.203.211:mxml
LIBMXML_NAME = $(LIBMXML_NAME_APP)
LIBMXML_SOURCE_DIR = $(DEPS_ROOT_DIRS)$(LIBMXML_NAME)/
LIBMXML_BUILD_DIR = $(LIBMXML_SOURCE_DIR)build/
LIBMXML_INCLUDE_DIR = $(INCLUDE_DIR)$(LIBMXML_NAME_APP)
#-----------------------------------------------------------

#mongo
LIBBSON_NAME_APP=bson
LIBBSON_NAME=lib$(LIBBSON_NAME_APP)
LIBBSON_INCLUDE_DIR=$(INCLUDE_DIR)$(LIBBSON_NAME)

LIBMONGO_NAME_APP = mongo
LIBMONGO_NAME = lib$(LIBMONGO_NAME_APP)
LIBMONGO_SOURCE_DIR = $(DEPS_ROOT_DIRS)$(LIBMONGO_NAME)/
LIBMONGO_BUILD_DIR = $(LIBMONGO_SOURCE_DIR)build/
LIBMONGO_INCLUDE_DIR=$(INCLUDE_DIR)$(LIBMONGO_NAME)/

#-----------------------------------------------------------

#mongo


# dependent.
DEPS_ROOT_DIRS = $(ROOT_DIR)deps/
ALL_DEPS_SUB_DIRS := $(shell find $(DEPS_ROOT_DIRS) -maxdepth 1 -type d)
ALL_DEPS_SUB_DIRS := $(filter-out $(DEPS_ROOT_DIRS),$(ALL_DEPS_SUB_DIRS))
ALL_DEPS_LIBS = $(LIBUV_GIT_ADDR) $(GYP_GIT_ADDR) $(LIBMYSQL_GIT_ADDR) $(LIBMXML_GIT_ADDR)
# --------------------------------------------------------


# css4.0
CXX   =  gcc
OPTI  = -o3
CXXFLAGS = -Wall -I. -fPIC -fprofile-arcs -ftest-coverage
INCPATHS = -I$(SRC_DIR) -I$(INCLUDE_DIR) -I$(LIBMYSQL_INCLUDE_DIR) -I$(LIBBSON_INCLUDE_DIR) -I$(LIBMONGO_INCLUDE_DIR)
LIBS =-l$(LIBUV_NAME_APP) -l$(LIBMYSQL_NAME_APP) -l$(LIBMXML_NAME_APP) -l$(LIBBSON_NAME_APP)-1.0 -l$(LIBMONGO_NAME_APP)c-1.0


all:compile_test_css
	@echo start css4.0 make...
	
del_deps:
	@echo clean all deps dirs of $(ALL_DEPS_SUB_DIRS)
	
	$(foreach dir,$(ALL_DEPS_SUB_DIRS),rm -rf $(dir);)
	
get_deps:del_deps
	
	@git submodule init && git submodule update
	mkdir -p $(LIBUV_SOURCE_BUILD_DIR) && cp -R $(GYP_SOURCE_DEPS_DIR)../gyp $(LIBUV_SOURCE_BUILD_DIR)
	
compile_mongo_c_driver:
	@echo compile mongo-c-driver lib ...
	rm -rf $(BIN_DIR)libbson*
	rm -rf $(BIN_DIR)libbmongoc*
	rm -rf $(LIBMONGO_SOURCE_DIR)src/$(LIBBSON_NAME)/
	rm -rf $(LIBMONGO_SOURCE_DIR)README
	rm -rf $(LIBMONGO_SOURCE_DIR)COPYING
	rm -rf $(LIBMONGO_SOURCE_DIR)src/$(LIBBSON_NAME)/README
	cp -r $(DEPS_ROOT_DIRS)$(LIBBSON_NAME) $(LIBMONGO_SOURCE_DIR)src/
	cd $(LIBMONGO_SOURCE_DIR)src/$(LIBBSON_NAME)&&ln -s README.md README
	cd $(LIBMONGO_SOURCE_DIR)&&ln -s README.rst README
	cd $(LIBMONGO_SOURCE_DIR)&&ln -s README.rst COPYING
	
	cd $(LIBMONGO_SOURCE_DIR)&&autoreconf --force --verbose --install -I ./build/autotools\
			&&rm -rf autom4te.cache&&./configure --prefix=$(LIBMONGO_BUILD_DIR)&&make&&make install
	mkdir -p $(INCLUDE_DIR)
	rm -rf $(LIBMONGO_INCLUDE_DIR)
	rm -rf $(LIBBSON_INCLUDE_DIR)
	mkdir -p $(LIBMONGO_INCLUDE_DIR)
	mkdir -p $(LIBBSON_INCLUDE_DIR)
	
	cp -r $(LIBMONGO_BUILD_DIR)include/libbson-1.0/*.h $(LIBBSON_INCLUDE_DIR)
	cp -r $(LIBMONGO_BUILD_DIR)include/libmongoc-1.0/*.h $(LIBMONGO_INCLUDE_DIR)
	
	cp -r $(LIBMONGO_BUILD_DIR)lib/libbson* $(BIN_DIR)
	cp -r $(LIBMONGO_BUILD_DIR)lib/libmongoc-1* $(BIN_DIR)
	
compile_mxml:
	mkdir -p $(BIN_DIR)
	@echo compile mxml lib ...
	rm -rf $(BIN_DIR)libmxml*
	mkdir -p $(INCLUDE_DIR)
	rm -rf $(LIBMXML_INCLUDE_DIR)
	mkdir -p $(LIBMXML_INCLUDE_DIR)
	cp $(LIBMXML_SOURCE_DIR)*.h $(LIBMXML_INCLUDE_DIR)
	mkdir -p $(LIBMXML_BUILD_DIR)
	cd $(LIBMXML_SOURCE_DIR)&&./configure --prefix=$(LIBMXML_BUILD_DIR)&&make&&make install
	cp -r $(LIBMXML_BUILD_DIR)/lib/libmxml.so* $(BIN_DIR)
	
compile_uv:
	@echo compile libuv lib ...
	mkdir -p $(BIN_DIR)
	mkdir -p $(INCLUDE_DIR)
	rm -rf $(LIBUV_INCLUDE_DIR)
	cp -r $(LIBUV_INCLUDE_SOURCE_DIR) $(LIBUV_INCLUDE_DIR)
	mkdir -p $(LIBUV_SOURCE_BUILD_DIR)
	cd $(LIBUV_SOURCE_DIR)&&sh autogen.sh&&./configure --prefix=$(LIBUV_SOURCE_BUILD_DIR)&&make&&make install\
					&&cp -r $(LIBUV_SOURCE_BUILD_DIR)lib/libuv.so* $(BIN_DIR)

compile_mysql:
	@echo compile mysql lib ...
	mkdir -p $(BIN_DIR)
	rm -rf $(BIN_DIR)$(LIBMYSQL_NAME)*
	mkdir -p $(INCLUDE_DIR)
	mkdir -p $(LIBMYSQL_INCLUDE_DIR)
	mkdir -p $(LIBMYSQL_BUILD_DIR)
	rm -rf $(LIBMYSQL_INCLUDE_DIR)
	cd $(LIBMYSQL_SOURCE_DIR)&&cmake -DCMAKE_INSTALL_PREFIX=$(LIBMYSQL_BUILD_DIR)\
			&&make mysqlclient libmysql&&make install&&cp -r $(LIBMYSQL_BUILD_DIR)include/ $(LIBMYSQL_INCLUDE_DIR)\
			&&cp $(LIBMYSQL_BUILD_DIR)lib/libmysql*.so* $(BIN_DIR)
					
					
compile_css:
	@echo compile css4.0 ..
	rm -fr $(EXE)
	$(CXX) -o $(EXE) $(SRC_FILES) $(INCPATHS) $(LIBS) -L$(LIB_PATH)   -Wl,-rpath=$(LIB_PATH)
	
compile_test_css:
	@echo compile css4.0 with test..
	rm -fr $(EXE_TEST)
	$(CXX) -g -o $(EXE_TEST) -DX64 -DCSS_TEST  $(SRC_FILES) $(INCPATHS) $(LIBS) -L$(LIB_PATH)  -Wl,-rpath=$(LIB_PATH)
	#cd $(BIN_DIR)&&$(EXE_TEST)

	
compile_all:compile_mongo_c_driver compile_uv compile_mysql compile_mxml compile_css
	@echo compile all of css4.0 ...
	
clean_all:
	@echo clean all css4.0 ...
	rm -rf $(BIN_DIR)*
	
clean:
	@echo clean css4.0 ...
	rm -rf $(BIN_DIR)css*
	
	
