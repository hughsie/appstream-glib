msgfmt ru.po -o ru.mo
msgfmt en_GB.po -o en_GB.mo
gcc -o app.bin app.c `pkg-config --cflags --libs gtk+-3.0`
cp * ~/rpmbuild/SOURCES/
rpmbuild -ba ../app.spec
rpmbuild -ba ../driver.spec
rpmbuild -ba ../composite.spec
cp ~/rpmbuild/RPMS/app*.rpm ../
cp ~/rpmbuild/RPMS/driver*.rpm ../
cp ~/rpmbuild/RPMS/composite*.rpm ../
