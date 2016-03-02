lib_LTLIBRARIES=libkayrebt_pathexaminer2.la
libkayrebt_pathexaminer2_la_SOURCES= constraint.cpp\
	constraint.h         \
	evaluator.cpp        \
	evaluator.h          \
	identifier.cpp       \
	identifier.h         \
	integer_cst.cpp      \
	integer_cst.h        \
	oracle.h             \
	path.h               \
	path_evaluator.cpp   \
	rich_basic_block.cpp \
	rich_basic_block.h   \
	ssa_name.cpp         \
	ssa_name.h           \
	string_cst.cpp       \
	string_cst.h         \
	value.cpp            \
	value.h              \
	value_factory.cpp    \
	value_factory.h      \
	bad_tree_exception.h \
	bad_tree_exception.cpp\
	configuration.h
libkayrebt_pathexaminer2_la_CXXFLAGS = -std=c++11 -I @gcc_includes@
libkayrebt_pathexaminer2_la_LDFLAGS  = -version-info 1:0:0
#libkayrebt_pathexaminer2_la_LIBADD =

install-exec-local:
	$(RM) `$(CC) -print-file-name=plugin`/kayrebt_pathexaminer2.so || true
	$(LN_S) @libdir@/libkayrebt_pathexaminer2.so `$(CC) -print-file-name=plugin`/kayrebt_pathexaminer2.so || echo "Couldn't install plugin in `$(CC) -print-file-name=plugin`"

