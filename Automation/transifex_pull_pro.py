#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2016, Psiphon Inc.
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

'''
Pulls and massages our translations from Transifex.
'''

import os
import errno
import shutil
import json
import codecs
import requests
from BeautifulSoup import BeautifulSoup

import psi_feedback_templates


DEFAULT_LANGS = {'ar': 'ar', 'es': 'es', 'fa': 'fa', 'kk': 'kk',
                 'ru': 'ru', 'th': 'th', 'tk': 'tk', 'vi': 'vi', 'zh': 'zh',
                 'ug': 'ug@Latn', 'nb_NO': 'nb', 'tr': 'tr', 'fr': 'fr',
                 'de': 'de', 'ko': 'ko', 'fi_FI': 'fi', 'el_GR': 'el',
                 'hr': 'hr', 'pt_PT': 'pt_PT', 'pt_BR': 'pt_BR', 'id': 'id'}
# Transifex does not support multiple character sets for Uzbek, but
# Psiphon supports both uz@Latn and uz@cyrillic. So we're going to
# use "Uzbek" ("uz") for uz@Latn and "Klingon" ("tlh") for uz@cyrillic.
# We opened an issue with Transifex about this, but it hasn't been
# rectified yet:
# https://getsatisfaction.com/indifex/topics/uzbek_cyrillic_language
DEFAULT_LANGS['uz'] = 'uz@Latn'
DEFAULT_LANGS['tlh'] = 'uz@cyrillic'


RTL_LANGS = ('ar', 'fa', 'he')


# There should be no more or fewer Transifex resources than this. Otherwise
# one or the other needs to be updated.
known_resources = \
    ['psiphon-pro-android-strings', 'descriptiontxt-1']


def process_psiphon_pro_android_strings():
    langs = {'ar': 'ar', 'de': 'de', 'es': 'es', 'fr': 'fr', 'id': 'id',
             'pt_BR': 'pt-rBR', 'ru': 'ru', 'tr': 'tr'}
    process_resource('psiphon-pro-android-strings',
                     lambda lang: '../Android/app/src/main/res/values-%s/pro-strings.xml' % lang,
                     None,
                     bom=False,
                     langs=langs)


def process_resource(resource, output_path_fn, output_mutator_fn, bom,
                     langs=None, skip_untranslated=False):
    '''
    `output_path_fn` must be callable. It will be passed the language code and
    must return the path+filename to write to.
    `output_mutator_fn` must be callable. It will be passed the output and the
    current language code. May be None.
    If `skip_untranslated` is True, translations that are less than 10% complete
    will be skipped.
    '''
    if not langs:
        langs = DEFAULT_LANGS

    for in_lang, out_lang in langs.items():
        if skip_untranslated:
            stats = request('resource/%s/stats/%s' % (resource, in_lang))
            if int(stats['completed'].rstrip('%')) < 10:
                continue

        r = request('resource/%s/translation/%s' % (resource, in_lang))

        if output_mutator_fn:
            # Transifex doesn't support the special character-type
            # modifiers we need for some languages,
            # like 'ug' -> 'ug@Latn'. So we'll need to hack in the
            # character-type info.
            content = output_mutator_fn(r['content'], out_lang)
        else:
            content = r['content']

        # Make line endings consistently Unix-y.
        content = content.replace('\r\n', '\n')

        output_path = output_path_fn(out_lang)

        # Path sure the output directory exists.
        try:
            os.makedirs(os.path.dirname(output_path))
        except OSError as ex:
            if ex.errno == errno.EEXIST and os.path.isdir(os.path.dirname(output_path)):
                pass
            else:
                raise

        with codecs.open(output_path, 'w', 'utf-8') as f:
            if bom:
                f.write(u'\uFEFF')
            f.write(content)


def gather_resource(resource, langs=None, skip_untranslated=False):
    '''
    Collect all translations for the given resource and return them.
    '''
    if not langs:
        langs = DEFAULT_LANGS

    result = {}
    for in_lang, out_lang in langs.items():
        if skip_untranslated:
            stats = request('resource/%s/stats/%s' % (resource, in_lang))
            if stats['completed'] == '0%':
                continue

        r = request('resource/%s/translation/%s' % (resource, in_lang))
        result[out_lang] = r['content'].replace('\r\n', '\n')

    return result


def check_resource_list():
    r = request('resources')
    available_resources = [res['slug'] for res in r]
    available_resources.sort()
    known_resources.sort()
    return available_resources == known_resources


# Initialized on first use.
_config = None


def request(command, params=None):
    global _config
    if not _config:
        # Must be of the form:
        # {"username": ..., "password": ...}
        with open('./transifex_pro_conf.json') as config_fp:
            _config = json.load(config_fp)

    url = 'https://www.transifex.com/api/2/project/psiphon-pro/' + command + '/'
    r = requests.get(url, params=params,
                     auth=(_config['username'], _config['password']))
    if r.status_code != 200:
        raise Exception('Request failed with code %d: %s' %
                            (r.status_code, url))
    return r.json()


def yaml_lang_change(in_yaml, to_lang):
    return to_lang + in_yaml[in_yaml.find(':'):]


def html_doctype_add(in_html, to_lang):
    return '<!DOCTYPE html>\n' + in_html


def go():
    if check_resource_list():
        print('Known and available resources match')
    else:
        raise Exception('Known and available resources do not match')

    process_psiphon_pro_android_strings()
    print('process_psiphon_pro_android_strings: DONE')


if __name__ == '__main__':
    if os.getcwd().split(os.path.sep)[-1] != 'Automation':
        raise Exception('Must be executed from Automation directory!')

    go()

    print('FINISHED')
