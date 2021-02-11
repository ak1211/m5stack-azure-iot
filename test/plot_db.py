#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.dates import DayLocator, HourLocator, DateFormatter
from pytz import timezone
from datetime import datetime
from dateutil import parser


def take_all_items_from_container(container):
    item_list = list(container.read_all_items())
    print('Found {} items'.format(item_list.__len__()))

    pairs = [('measuredAt', parser.parse),
             ('temperature', float),
             ('humidity', float),
             ('pressure', float)]
    columns, conversions = zip(*pairs)

    def pickup(item):
        return [conv(item.get(col)) for (col, conv) in pairs]
    #
    data = [pickup(item) for item in item_list]
    return pd.DataFrame(data=data, columns=columns)


def plot(container):
    df = take_all_items_from_container(container)
    print(df)
    #
    tz = timezone('Asia/Tokyo')
    major_formatter = DateFormatter('\n%Y-%m-%d\n%H:%M:%S\n%Z', tz=tz)
    minor_formatter = DateFormatter('%H', tz=tz)
    #
    fig, axs = plt.subplots(2, 2, figsize=(48.0, 48.0))
    #
    axs[0, 0].xaxis.set_major_locator(HourLocator(interval=12))
    axs[0, 0].xaxis.set_major_formatter(major_formatter)
    axs[0, 0].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[0, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 0].set_ylabel('$^{\circ}C$')
    axs[0, 0].set_title('temperature', fontsize=28)
    axs[0, 0].plot(df['measuredAt'], df['temperature'],
                   label='temperature[$^{\circ}C$]')
    axs[0, 0].grid()
    axs[0, 0].legend(fontsize=18)
    #
    axs[0, 1].xaxis.set_major_locator(HourLocator(interval=12))
    axs[0, 1].xaxis.set_major_formatter(major_formatter)
    axs[0, 1].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[0, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 1].set_ylabel('%')
    axs[0, 1].set_title('relative humidity', fontsize=28)
    axs[0, 1].plot(df['measuredAt'], df['humidity'],
                   label='relative humidity[%]')
    axs[0, 1].grid()
    axs[0, 1].legend(fontsize=18)
    #
    axs[1, 0].xaxis.set_major_locator(HourLocator(interval=12))
    axs[1, 0].xaxis.set_major_formatter(major_formatter)
    axs[1, 0].xaxis.set_minor_locator(HourLocator(interval=1))
    axs[1, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 0].set_ylabel('hPa')
    axs[1, 0].set_title('pressure', fontsize=28)
    axs[1, 0].plot(df['measuredAt'], df['pressure'], label='pressure[hPa]')
    axs[1, 0].grid()
    axs[1, 0].legend(fontsize=18)
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
