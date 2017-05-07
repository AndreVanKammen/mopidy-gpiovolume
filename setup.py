from __future__ import unicode_literals

import re

from setuptools import find_packages, setup


def get_version(filename):
    with open(filename) as fh:
        metadata = dict(re.findall("__([a-z]+)__ = '([^']+)'", fh.read()))
        return metadata['version']


setup(
    name='mopidy-gpiovolume',
    version=get_version('mopidy_gpiovolume/__init__.py'),
    url='https://github.com/AndreVanKammen/mopidy-gpiovolume',
    license='Apache License, Version 2.0',
    author='Andre van Kammen',
    author_email='andrevankammen@gmail.com',
    description='Mopidy extension for volume and standby control of a hacked Logitech Z-680 through GPIO',
    long_description=open('README.rst').read(),
    packages=find_packages(exclude=['tests', 'tests.*']),
    zip_safe=False,
    include_package_data=True,
    install_requires=[
        'setuptools',
        'Mopidy >= 1.0',
        'Pykka >= 1.1',
    ],
    entry_points={
        'mopidy.ext': [
            'gpiovolume = mopidy_gpiovolume:Extension',
        ],
    },
    classifiers=[
        'Environment :: No Input/Output (Daemon)',
        'Intended Audience :: End Users/Desktop',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Topic :: Multimedia :: Sound/Audio :: Players',
    ],
)
