# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#

import subprocess

subprocess.call('cd ../../doxygen && doxygen Doxyfile && cd ../sphinx/source', shell=True)


# -- Project information -----------------------------------------------------

project = 'Blosc2'
copyright = '2021, The Blosc Developers'
author = 'The Blosc Developers'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'breathe',
    'sphinx.ext.intersphinx',
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "pydata_sphinx_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

html_css_files = [
    'css/custom.css',
]

html_logo = "_static/blosc-logo_256.png"

html_theme_options = {
    "external_links": [
        {"name": "Python Library", "url": "https://python-blosc2.readthedocs.io"},
    ],
    "github_url": "https://github.com/Blosc/c-blosc2",
    "twitter_url": "https://twitter.com/Blosc2",
}

# -- Breathe configuration ---------------------------------------------------

breathe_projects = {
    "blosc2": "../../doxygen/xml/",
}
breathe_default_project = "blosc2"
breathe_show_define_initializer = True
breathe_order_parameters_first = True
breathe_domain_by_extension = {"h": "c"}
