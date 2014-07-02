#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# Copyright (C) 2009-2014
#    Richard Hughes <richard@hughsie.com>
#

import glob
import os
import rpm
import rpmUtils
import sys
import yum
import fnmatch
import datetime
import ConfigParser

timestamp = datetime.datetime.now().strftime('%Y%m%d')

_ts = rpm.ts()
_ts.setVSFlags(0x7FFFFFFF)

class Config:

    def __init__(self):

        # get the project defaults
        self.cfg_project = ConfigParser.ConfigParser()
        self.cfg_project.read('./project.conf')
        self.distro_tag = self.cfg_project.get('AppstreamProject', 'DistroTag')
        self.repo_ids = self.cfg_project.get('AppstreamProject', 'RepoIds').split(',')

def _do_newest_filtering(pkglist):
    '''
    Only return the newest package for each name.arch
    '''
    newest = {}
    for pkg in pkglist:
        key = (pkg.name, pkg.arch)
        if key in newest:

            # the current package is older
            if pkg.verCMP(newest[key]) < 0:
                continue

            # the current package is the same version
            if pkg.verCMP(newest[key]) == 0:
                continue

            # the current package is newer than what we have stored
            del newest[key]

        newest[key] = pkg
    return newest.values()

def search_package_list(pkg):
    for instfile in pkg.returnFileEntries():
        if fnmatch.fnmatch(instfile, '/usr/share/applications/*.desktop'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/applications/kde4/*.desktop'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/fonts/*/*.otf'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/fonts/*/*.ttf'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/ibus/component/*.xml'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/ibus-table/tables/*.db'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/lib64/gstreamer-1.0/libgst*.so'):
            return True
        if fnmatch.fnmatch(instfile, '/usr/share/appdata/*.metainfo.xml'):
            return True
    return False

def update():

    # create if we're starting from nothing
    if not os.path.exists('./packages'):
        os.makedirs('./packages')

    # get extra packages needed for some applications
    cfg = Config()
    extra_packages = []
    extra_packages.append('alliance')
    extra_packages.append('calligra-core')
    extra_packages.append('coq')
    extra_packages.append('efte-common')
    extra_packages.append('fcitx-data')
    extra_packages.append('gcin-data')
    extra_packages.append('hotot-common')
    extra_packages.append('java-1.7.0-openjdk')
    extra_packages.append('kchmviewer')
    extra_packages.append('libprojectM-qt')
    extra_packages.append('libreoffice-core')
    extra_packages.append('nntpgrab-core')
    extra_packages.append('scummvm')
    extra_packages.append('speed-dreams-robots-base')
    extra_packages.append('switchdesk')
    extra_packages.append('transmission-common')
    extra_packages.append('vegastrike-data')

    # find out what we've got already
    files = glob.glob("./packages/*.rpm")
    files.sort()
    existing = {}
    for f in files:
        fd = os.open(f, os.O_RDONLY)
        try:
            hdr = _ts.hdrFromFdno(fd)
        except Exception as e:
            pass
        else:
            existing[hdr.name] = f
        os.close(fd)
    print("INFO: found %i existing packages for %s" % (len(existing), cfg.distro_tag))

    # setup yum
    yb = yum.YumBase()
    yb.preconf.releasever = cfg.distro_tag[1:]
    yb.doConfigSetup(errorlevel=-1, debuglevel=-1)
    yb.conf.cache = 0

    # what is native for this arch
    basearch = rpmUtils.arch.getBaseArch()
    if basearch == 'i386':
        basearch_list = ['i386', 'i486', 'i586', 'i686']
    else:
        basearch_list = [basearch]
    basearch_list.append('noarch')

    # ensure all the repos are enabled
    for repo_id in cfg.repo_ids:
        repo = yb.repos.getRepo(repo_id)
        repo.enable()

    # reget the metadata every day
    for repo in yb.repos.listEnabled():
        repo.metadata_expire = 60 * 60 * 12  # 12 hours

    # find all packages
    downloaded = {}
    try:
        pkgs = yb.pkgSack
    except yum.Errors.NoMoreMirrorsRepoError as e:
        print("FAILED:" % str(e))
        sys.exit(1)
    newest_packages = _do_newest_filtering(pkgs)
    newest_packages.sort()
    for pkg in newest_packages:

        # not our repo
        if pkg.repoid not in cfg.repo_ids:
            continue

        # not our arch
        if pkg.arch not in basearch_list:
            continue

        # make sure the metadata exists
        repo = yb.repos.getRepo(pkg.repoid)

        # don't download packages without desktop files
        if not search_package_list(pkg) and pkg.name not in extra_packages:
            continue

        # get base name without the slash
        relativepath = pkg.returnSimple('relativepath')
        pos = relativepath.rfind('/')
        if pos != -1:
            relativepath = relativepath[pos+1:]

        # is in cache?
        path = './packages/' + relativepath
        if os.path.exists(path) and os.path.getsize(path) == int(pkg.returnSimple('packagesize')):
            #print("INFO: %s up to date" % pkg.nvra)
            downloaded[pkg.name] = True
        else:
            pkg.localpath = path

            # download now
            print("INFO: downloading %s" % os.path.basename(path))
            repo.getPackage(pkg)

            # do we have an old version of this?
            if existing.has_key(pkg.name) and os.path.exists(existing[pkg.name]):
                print("INFO: deleting %s" % os.path.basename(existing[pkg.name]))
                os.remove(existing[pkg.name])
        downloaded[pkg.name] = True

    if len(downloaded) == 0:
        print("INFO: no packages downloaded for %s" % cfg.distro_tag)
        return

    # have any packages been removed?
    for i in existing:
        if not downloaded.has_key(i):
            print("INFO: deleting %s" % existing[i])
            os.remove(existing[i])

def main():

    # check we're not top level
    if os.path.exists('./application.py'):
        print 'You cannot run these tools from the top level directory'
        sys.exit(1)

    # update all the packages
    update()
    sys.exit(0)

if __name__ == "__main__":
    main()
