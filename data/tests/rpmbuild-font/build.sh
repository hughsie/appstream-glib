cp * ~/rpmbuild/SOURCES/
rpmbuild -ba ../font.spec
cp ~/rpmbuild/RPMS/font*.rpm ../
