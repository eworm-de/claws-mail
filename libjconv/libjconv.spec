Summary: Japanese Code Conversion Library
Name: libjconv
Version: 2.8.1
Release: 2k
License: LGPL
Group: System Environment/Libraries
Source: %{name}-%{version}.tar.gz
URL: http://www.kondara.org/libjconv/
Buildroot: /var/tmp/libjconv-root
Requires: glibc >= 2.1.0

%description
This package provide Japanese Code Conversion capability based on iconv.

%prep
%setup

%build
make

%install
mkdir -p $RPM_BUILD_ROOT/usr/{lib,include,bin}
mkdir -p $RPM_BUILD_ROOT/etc/libjconv
install -m 755 libjconv.so $RPM_BUILD_ROOT/usr/lib
install -m 644 libjconv.a $RPM_BUILD_ROOT/usr/lib
install -m 755 jconv $RPM_BUILD_ROOT/usr/bin
install -m 644 jconv.h $RPM_BUILD_ROOT/usr/include
install -m 644 default.conf $RPM_BUILD_ROOT/etc/libjconv

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc README COPYING
/usr/lib/libjconv.so
/usr/lib/libjconv.a
/usr/bin/jconv
/usr/include/jconv.h
%config /etc/libjconv

%changelog
* Wed Nov 21 2001 Shingo Akagaki <dora@kondara.org>
- version 2.8.1
- fixup README from vine-users:030436

* Sat Aug  5 2000 Akira Higuchi <a@kondara.org>
- version 2.8
- use nl_langinfo(CODESET)

* Fri Jun 30 2000 Akira Higuchi <a@kondara.org>
- version 2.7
- added data for UTF-8 locales

* Fri Apr 14 2000 Akira Higuchi <a@kondara.org>
- version 2.5

* Thu Apr  6 2000 Akira Higuchi <a@kondara.org>
- version 2.3

* Sun Feb 27 2000 Akira Higuchi <a@kondara.org>
- added libjconv.a
- fixed a bug in jconv_strdup_conv_autodetect()

* Mon Feb  7 2000 Akira Higuchi <a@kondara.org>
- major version up.

* Sun Feb  6 2000 Shingo Akagaki <dora@kondara.org>
- version up.

* Wed Jan 15 2000 Toru Hoshina <t@kondara.org>
- version up.

* Sat Dec 11 1999 Akira Higuchi <a@kondara.org>
- convert_kanji(): write reset sequences for state-dependent encodings

* Mon Nov  8 1999 Toru Hoshina <t@kondara.org>
- be a NoSrc :-P

* Wed Sep 29 1999 Toru Hoshina <hoshina@best.com>
- fixed Nie-a bug :-P

* Thu Sep 23 1999 Toru Hoshina <hoshina@best.com>
- localedata-ja is no longer required because we decide to provide our own
  glibc including localedata-ja !!

* Sun Aug 29 1999 Toru Hoshina <hoshina@best.com>
- 1st release.
