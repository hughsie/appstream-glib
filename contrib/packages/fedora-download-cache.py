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
import hashlib
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

def get_sha256_hash(filename, block_size=256*128):
    md5 = hashlib.sha256()
    with open(filename,'rb') as f:
        for chunk in iter(lambda: f.read(block_size), b''):
             md5.update(chunk)
    return md5.hexdigest()

def ensure_pkg_exists(yb, existing, pkg):

    # get base name without the slash
    relativepath = pkg.returnSimple('relativepath')
    pos = relativepath.rfind('/')
    if pos != -1:
        relativepath = relativepath[pos+1:]

    # is in cache?
    path = './packages/' + relativepath
    if os.path.exists(path) and get_sha256_hash(path) == pkg.checksum:
        #print("INFO: %s up to date" % pkg.nvra)
        return

    # do we have an old version of this?
    if existing.has_key(pkg.name) and os.path.exists(existing[pkg.name]):
        print("INFO: deleting %s" % os.path.basename(existing[pkg.name]))
        os.remove(existing[pkg.name])

    # make sure the metadata exists
    repo = yb.repos.getRepo(pkg.repoid)

    # download now
    print("INFO: downloading %s" % os.path.basename(path))
    pkg.localpath = path
    repo.getPackage(pkg)

def pkg_from_name(sack, name):
    for pkg in sack:
        if pkg.name == name:
            return pkg
    return None

def update():

    # create if we're starting from nothing
    if not os.path.exists('./packages'):
        os.makedirs('./packages')

    # get extra packages needed for some applications
    cfg = Config()
    extra_packages = []
    extra_packages.append('alliance')
    extra_packages.append('hotot-common')
    extra_packages.append('java-1.7.0-openjdk')
    extra_packages.append('kchmviewer')
    extra_packages.append('libprojectM-qt')
    extra_packages.append('nntpgrab-core')
    extra_packages.append('scummvm')
    extra_packages.append('switchdesk')
    extra_packages.append('transmission-common')
    extra_packages.append('oxygen-icon-theme')

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
    print("INFO: Found %i existing packages for %s" % (len(existing), cfg.distro_tag))

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

    # reget the correct metadata every day
    yb.repos.enableRepo('rawhide')
    for repo in yb.repos.listEnabled():
        if repo.id in cfg.repo_ids:
            repo.enable()
            repo.metadata_expire = 60 * 60 * 12  # 12 hours
            print("INFO: enabled: %s" % repo.id)
        else:
            repo.disable()
            print("INFO: disabled: %s" % repo.id)

    # find all packages
    print("INFO: Checking metadata...")
    downloaded = {}
    try:
        pkgs = yb.pkgSack
    except yum.Errors.NoMoreMirrorsRepoError as e:
        print("FAILED:" % str(e))
        sys.exit(1)
    newest_packages = _do_newest_filtering(pkgs)

    # restrict this down so we have less to search
    print("INFO: Finding interesting packages...")
    available_packages = []
    matched_packages = []
    for pkg in newest_packages:
        if pkg.repoid in cfg.repo_ids and pkg.arch in basearch_list:
            available_packages.append(pkg)
            if pkg.name in extra_packages or search_package_list(pkg):
                matched_packages.append(pkg)

    # ensure the package, and the first level of deps is downloaded
    print("INFO: Downloading packages...")
    matched_packages.sort()
    for pkg in matched_packages:
        #print("INFO: Checking for %s..." % pkg.name)
        ensure_pkg_exists(yb, existing, pkg)
        for require in pkg.strong_requires_names:
            # remove helpful package suffixes
            idx = require.find('(x86-64)')
            if idx > 0:
                require = require[0:idx]
            if downloaded.has_key(require):
                continue
            dep = pkg_from_name(available_packages, require)
            if not dep:
                continue
            if dep.base_package_name != pkg.base_package_name and not require.startswith(pkg.name):
                continue
            #print ("INFO: " + pkg.name + " also needs " + require)
            if require in extra_packages:
                print("WARNING: Remove %s from whitelist" % require)
            ensure_pkg_exists(yb, existing, dep)
            downloaded[dep.name] = True
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
