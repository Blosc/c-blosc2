# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
import pathlib
import subprocess


subprocess.run('doxygen Doxyfile',
               shell=True, check=True)


project = 'C-Blosc2'
copyright = '2019-present, Blosc Development Team'
author = 'Blosc Development Team'
extensions = [
    'breathe',
    'sphinx.ext.intersphinx',
]
html_theme = "pydata_sphinx_theme"
html_static_path = ['_static']
html_css_files = [
    'css/custom.css',
]
html_logo = "_static/blosc-logo_256.png"
html_theme_options = {
    "logo": {
        "link": "/index",
        "alt_text": "Blosc",
    },
    "external_links": [
        {"name": "Python-Blosc", "url": "/python-blosc/python-blosc.html"},
        {"name": "Python-Blosc2", "url": "/python-blosc2/python-blosc2.html"},
        {"name": "Blosc In Depth", "url": "/pages/blosc-in-depth/"},
        {"name": "Donate to Blosc", "url": "/pages/donate/"},
    ],
    "github_url": "https://github.com/Blosc/c-blosc2",
    "twitter_url": "https://twitter.com/Blosc2",
}
html_show_sourcelink = False
breathe_projects = {
    "blosc2": pathlib.Path(__file__).parent.resolve() / "xml/",
}
breathe_default_project = "blosc2"
breathe_show_define_initializer = True
breathe_order_parameters_first = True
breathe_domain_by_extension = {"h": "c"}
