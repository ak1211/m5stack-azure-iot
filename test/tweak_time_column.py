#!/usr/bin/env python3
# CSVファイルの時間をISO8601形式にする
#
# Copyright (c) 2022 Akihiro Yamamoto.
# Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
# See LICENSE file in the project root for full license information.
import os
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pytz import timezone
from datetime import datetime, timedelta
from dateutil import parser

def run():
    for input_filename in glob.glob("./*.csv"):
        output_filename = os.path.splitext(input_filename)[0] + ".csv"
        print(input_filename)
        print(output_filename)
        # 同名のファイルがあれば何もしない
        if (os.path.isfile(output_filename)):
            print("file {} is already exist, pass".format(output_filename))
        else:
            df = pd.read_csv(input_filename, dtype = {
                'measuredAt':           'object',
                'messageId':            'object',
                'sensorId':             'object',
                'temperature':          'float64',
                'humidity':             'float64',
                'pressure':             'float64',
                'tvoc':                 'float64',
                'eCo2':                 'float64',
                'co2':                  'float64',
                'absolute_humidity':    'float64',
                })
            tz = timezone('Asia/Tokyo')
            df["measuredAt"]= pd.to_datetime(df["measuredAt"])
            df = df.set_index('measuredAt')
            df.to_csv(output_filename, date_format='%Y-%m-%dT%H:%M:%S%z')
    
if __name__ == '__main__':
    run()
