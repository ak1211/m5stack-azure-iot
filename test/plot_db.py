#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math
from matplotlib.dates import DayLocator, HourLocator, DateFormatter
from pytz import timezone
from datetime import datetime
from dateutil import parser


def take_all_items_from_container(container):
    item_list = list(container.read_all_items())
    print('Found {} items'.format(item_list.__len__()))

    pairs = [('sensorId', lambda x:x),
             ('measuredAt', parser.parse),
             ('temperature', lambda x: float(x) if x is not None else None),
             ('humidity', lambda x: float(x) if x is not None else None),
             ('pressure', lambda x: float(x) if x is not None else None),
             ('tvoc', lambda x: int(x) if x is not None else None),
             ('eCo2', lambda x: int(x) if x is not None else None)]
    columns, _ = zip(*pairs)

    def pickup(item):
        return [func(item.get(col)) for (col, func) in pairs]
    #
    data = [pickup(item) for item in item_list]
    df = pd.DataFrame(data=data, columns=columns)
    return df.set_index('measuredAt')


def calculate_absolute_humidity(df):
    def calc(temp, humi):
        # [g/m^3]
        absolute_humidity = 216.7 * ((humi / 100.0) * 6.112 *
                                     math.exp((17.62 * temp) / (243.12 + temp)) / (273.15 + temp))
        return absolute_humidity
        #
    df['absolute_humidity'] = list(
        map(calc, df['temperature'], df['humidity']))
    return df


def plot(container):
    df = take_all_items_from_container(container)
#    print(df)
    print(calculate_absolute_humidity(df))
    #
    tz = timezone('Asia/Tokyo')
    major_formatter = DateFormatter('\n%Y-%m-%d\n%H:%M:%S\n%Z', tz=tz)
    minor_formatter = DateFormatter('%H', tz=tz)
    #
    fig, axs = plt.subplots(3, 2, figsize=(72.0, 48.0))
    #
    axs[0, 0].xaxis.set_major_locator(HourLocator(interval=12))
    axs[0, 0].xaxis.set_major_formatter(major_formatter)
    axs[0, 0].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[0, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 0].set_ylabel('$^{\circ}C$')
    axs[0, 0].set_title('temperature', fontsize=28)
    axs[0, 0].plot(df['temperature'].dropna(), 'o-',
                   label='temperature[$^{\circ}C$]')
    axs[0, 0].grid()
    axs[0, 0].legend(fontsize=18)
    #
    axs[0, 1].xaxis.set_major_locator(HourLocator(interval=12))
    axs[0, 1].xaxis.set_major_formatter(major_formatter)
    axs[0, 1].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[0, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 1].set_ylabel('hPa')
    axs[0, 1].set_title('pressure', fontsize=28)
    axs[0, 1].plot(df['pressure'].dropna(), 's-', label='pressure[hPa]')
    axs[0, 1].grid()
    axs[0, 1].legend(fontsize=18)
    #
    axs[1, 0].xaxis.set_major_locator(HourLocator(interval=12))
    axs[1, 0].xaxis.set_major_formatter(major_formatter)
    axs[1, 0].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[1, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 0].set_ylabel('%RH')
    axs[1, 0].set_title('relative humidity', fontsize=28)
    axs[1, 0].plot(df['humidity'].dropna(), 's-',
                   label='relative humidity[%RH]')
    axs[1, 0].grid()
    axs[1, 0].legend(fontsize=18)
    #
    axs[1, 1].xaxis.set_major_locator(HourLocator(interval=12))
    axs[1, 1].xaxis.set_major_formatter(major_formatter)
    axs[1, 1].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[1, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 1].set_ylabel('$mg/m^3$')
    axs[1, 1].set_title('absolute humidity', fontsize=28)
    axs[1, 1].plot(df['absolute_humidity'].dropna(), 's-',
                   label='absolute humidity[$g/m^3$]')
    axs[1, 1].grid()
    axs[1, 1].legend(fontsize=18)
    #
    axs[2, 0].xaxis.set_major_locator(HourLocator(interval=12))
    axs[2, 0].xaxis.set_major_formatter(major_formatter)
    axs[2, 0].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[2, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 0].set_ylabel('ppm')
    axs[2, 0].set_title('equivalent CO2', fontsize=28)
    axs[2, 0].plot(df['eCo2'].dropna(), 's-', label='eCO2[ppm]')
    axs[2, 0].grid()
    axs[2, 0].legend(fontsize=18)
    #
    axs[2, 1].xaxis.set_major_locator(HourLocator(interval=12))
    axs[2, 1].xaxis.set_major_formatter(major_formatter)
    axs[2, 1].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[2, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 1].set_ylabel('ppb')
    axs[2, 1].set_title('Total VOC', fontsize=28)
    axs[2, 1].plot(df['tvoc'].dropna(), 's-', label='TVOC[ppb]')
    axs[2, 1].grid()
    axs[2, 1].legend(fontsize=18)
    #
#    fig.tight_layout()
    fig.savefig("plot.png")


def run(url, key):
    cosmos_client = CosmosClient(url, credential=key)
    database_name = "ThingsDatabase"
    container_name = "Measurements"
    database = cosmos_client.get_database_client(database_name)
    container = database.get_container_client(container_name)
    plot(container)


if __name__ == "__main__":
    if len(sys.argv) <= 2:
        print("{} url key".format(sys.argv[0]))
    else:
        url = sys.argv[1]
        key = sys.argv[2]
        run(url, key)
