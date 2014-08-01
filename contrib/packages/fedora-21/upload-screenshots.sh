chmod a+rx screenshots/*x*
chmod a+r screenshots/*x*/*.png
rsync -v --progress ./screenshots/ rhughes@secondary01.fedoraproject.org:/srv/pub/alt/screenshots/f21/
