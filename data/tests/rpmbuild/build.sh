msgfmt ru.po -o ru.mo
msgfmt en_GB.po -o en_GB.mo
gcc -o app.bin app.c `pkg-config --cflags --libs gtk+-3.0`
cp * ~/rpmbuild/SOURCES/ && rpmbuild -ba ../app.spec && cp ~/rpmbuild/RPMS/app*.rpm ../
