@echo off
rem create passcrypt.h from passcrypt.h.in using sed
set ROOT=..\..
sed -e "s/@PASSCRYPT_KEY@/passkey0/" %ROOT%\src\common\passcrypt.h.in > %ROOT%\src\common\passcrypt.h
set ROOT=
