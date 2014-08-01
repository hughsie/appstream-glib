chmod a+rx 112x63
chmod a+rx 624x351
chmod a+rx 752x423
chmod a+rx source
chmod a+r */*.png
chmod a+r *.html
rsync -v --progress 112x63 624x351 752x423 *.html applications-to-import.yaml	\
	rhughes@secondary01.fedoraproject.org:/srv/pub/alt/screenshots/f21/
