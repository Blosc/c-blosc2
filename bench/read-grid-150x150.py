"""
Copyright (C) 2021  The Blosc developers <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

Script to extract a small grid of precipitation out of this file:

ftp://ftp-cdc.dwd.de/pub/REA/COSMO_REA6/hourly/2D/TOT_PRECIP/TOT_PRECIP.2D.201512.grb.bz2

After downloading it and uncompressing it, just run this script for extracting a small grid.

For more info on these datasets, see http://reanalysis.meteo.uni-bonn.de.
"""

from osgeo import gdal
import numpy as np
import blosc

# Read a 150x150 grid from the first band
dataset = gdal.Open("TOT_PRECIP.2D.201512.grb", gdal.GA_ReadOnly)
band = dataset.GetRasterBand(1)
precip_data = band.ReadAsArray(0, 0, 150, 150).astype(np.float32)

# Compress the chunk and write it to a file
cdata = blosc.compress(precip_data, cname="blosclz", clevel=9, typesize=4)
open("rainfall-band-150x150.bin", "wb").write(cdata)
