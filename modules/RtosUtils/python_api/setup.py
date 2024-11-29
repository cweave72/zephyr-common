from setuptools import setup, find_packages

NAME = "rtosutils"
DESC = "RPC api for RtosUtils."
VERSION = "0.1.0"

required = [
    "click",
    "rich",
]

setup(
    name=NAME,
    version=VERSION,
    description=DESC,
    author='cdw',
    entry_points={
        'console_scripts': [
            'rtos_cli=rtosutils.cli:main'
        ],
    },
    packages=find_packages(),
    install_requires=required
)
