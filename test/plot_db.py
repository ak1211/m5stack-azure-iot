#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.dates import DayLocator, HourLocator, DateFormatter, drange
from datetime import datetime, timedelta


def take_all_items_from_container(container):
    item_list = list(container.read_all_items())
    index = ['measuredAt', 'temperature', 'humidity', 'pressure']

    def pickup(item):
        return list(map(item.get, index))
    #
    return pd.DataFrame(data=list(map(pickup, item_list)),
                        columns=index
                        )


def plot(container):
    df = take_all_items_from_container(container)
    print(df)
    at = [datetime.strptime(x, '%Y-%m-%dT%H:%M:%SZ') for x in df['measuredAt']]
    at_asia_tokyo = [x + timedelta(hours=9) for x in at]
    #
    fig, axs = plt.subplots(2, 2, figsize=(24.0, 24.0))
    #
    axs[0, 0].xaxis.set_major_formatter(DateFormatter('%Y-%m-%d\n%H:%M:%S'))
    axs[0, 0].set_xlabel('UTC+9, the asia/Tokyo time zone')
    axs[0, 0].set_ylabel('$^{\circ}C$')
    axs[0, 0].set_title('temperature', fontsize=28)
    axs[0, 0].plot(at_asia_tokyo, df['temperature'],
                   label='temperature[$^{\circ}C$]')
    axs[0, 0].grid()
    axs[0, 0].legend(fontsize=18)
    #
    axs[0, 1].xaxis.set_major_formatter(DateFormatter('%Y-%m-%d\n%H:%M:%S'))
    axs[0, 0].set_xlabel('UTC+9, the asia/Tokyo time zone')
    axs[0, 1].set_ylabel('%')
    axs[0, 1].set_title('relative humidity', fontsize=28)
    axs[0, 1].plot(at_asia_tokyo, df['humidity'], label='relative humidity[%]')
    axs[0, 1].grid()
    axs[0, 1].legend(fontsize=18)
    #
    axs[1, 0].xaxis.set_major_formatter(DateFormatter('%Y-%m-%d\n%H:%M:%S'))
    axs[0, 0].set_xlabel('UTC+9, the asia/Tokyo time zone')
    axs[1, 0].set_ylabel('hPa')
    axs[1, 0].set_title('pressure', fontsize=28)
    axs[1, 0].plot(at_asia_tokyo, df['pressure'], label='pressure[hPa]')
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
