
CC = gcc
OBJS = conv.o info.o compat.o
SRCS = $(OBJS:.o=.c)
CFLAGS += -Wall -O9 -fPIC -DHAVE_CODESET
#CFLAGS = -g -Wall -O0 -fPIC
PKGDIR = libjconv-2.8

all: jconv libjconv.so libjconv.a default.conf

jconv: jconv.o $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o jconv jconv.o $(OBJS)

libjconv.so: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,libjconv.so.0 -o libjconv.so $(OBJS)

libjconv.a: $(OBJS)
	ar rc libjconv.a $(OBJS)

clean:
	rm -rf $(OBJS) jconv.o *~ jconv $(PKGDIR) $(PKGDIR).tar.gz
	rm -f tmp.conf
	rm -f libjconv.so libjconv.a

dist: clean
	mkdir $(PKGDIR)
	cp *.c *.h Makefile *.spec COPYING README *.conf $(PKGDIR)
	tar cvfz $(PKGDIR).tar.gz $(PKGDIR)

default.conf:
	( cd /usr/share/i18n/locales/; \
	for i in ??_??; do grep "^% Charset" $$i /dev/null; done \
	| sed -e "s/% Charset//g" | sed -e "s/:[0-9]*$$//g" \
	| grep -v "^ko_KR" \
	| grep -v "^zh_CN" \
	| sed -e "s/:: \+/   : /g" \
	| sed -e "s/ISO_/ISO-/g" \
	) >> tmp.conf
	echo "# name  : codeset    [ file-codesets ... ]" > default.conf
	cat tmp.conf >> default.conf
	echo "ja_JP   : EUC-JP ISO-2022-JP ISO-2022-JP-2 EUC-JP SJIS" \
		>> default.conf
	echo "ko_KR   : EUC-KR ISO-2022-KR EUC-KR" \
		>> default.conf
	echo "zh_CN   : EUC-CN ISO-2022-CN EUC-CN GB2312" \
		>> default.conf
	echo "zh_TW   : EUC-TW ISO-2022-CN EUC-TW BIG5" \
		>> default.conf
	sed -e \
	"s/\\(.._..\\) \+: \+\\([^ ]\+\\)$$/\\1.UTF-8 : UTF-8 \\2 UTF-8/" \
		< tmp.conf >> default.conf
	echo "ja_JP.UTF-8 : UTF-8 ISO-2022-JP ISO-2022-JP-2 EUC-JP SJIS UTF-8" \
		>> default.conf
	echo "ko_KR.UTF-8 : UTF-8 ISO-2022-KR EUC-KR UTF-8" \
		>> default.conf
	echo "zh_CN.UTF-8 : UTF-8 ISO-2022-CN EUC-CN GB2312 UTF-8" \
		>> default.conf
	echo "zh_TW.UTF-8 : UTF-8 ISO-2022-CN EUC-TW BIG5 UTF-8" \
		>> default.conf

