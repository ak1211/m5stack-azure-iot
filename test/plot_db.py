#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math
import itertools
from matplotlib.dates import DayLocator, HourLocator, DateFormatter
from pytz import timezone
from datetime import datetime, timedelta
from dateutil import parser


def time_sequential_data_frame(item_list):
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


def take_all_items_from_container(container):
    item_list = list(container.read_all_items())
    return time_sequential_data_frame(item_list)


def take_items_from_container(container, begin, end):
    begin_ = begin.isoformat()
    end_ = end.isoformat()
    print("{} -> {}".format(begin_, end_))
    item_list = list(container.query_items(
        query="SELECT * FROM c WHERE c.measuredAt BETWEEN @begin AND @end",
        parameters=[
            {"name": "@begin", "value": begin_},
            {"name": "@end", "value": end_}
        ],
        enable_cross_partition_query=True))
    return time_sequential_data_frame(item_list)


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


def plot(df, filename):
    df = calculate_absolute_humidity(df)
    print(df)
    #
    tz = timezone('Asia/Tokyo')
    major_formatter = DateFormatter('\n%Y-%m-%d\n%H:%M:%S\n%Z', tz=tz)
    major_locator = DayLocator(interval=1, tz=tz)
    minor_formatter = DateFormatter('%H', tz=tz)
    minor_locator = HourLocator(byhour=range(0, 24, 6), tz=tz)
    #
    fig, axs = plt.subplots(4, 2, figsize=(32, 32))
    #
    axs[0, 0].xaxis.set_major_locator(major_locator)
    axs[0, 0].xaxis.set_major_formatter(major_formatter)
    axs[0, 0].xaxis.set_minor_locator(minor_locator)
    axs[0, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 0].set_ylim(-10.0, 60.0)
    axs[0, 0].set_ylabel('$^{\circ}C$')
    axs[0, 0].set_title('temperature', fontsize=18)
    axs[0, 0].plot(df['temperature'].dropna(), 'o-',
                   label='temperature[$^{\circ}C$]')
    axs[0, 0].grid(which='both', axis='both')
    axs[0, 0].legend(fontsize=18)
    #
    axs[0, 1].xaxis.set_major_locator(major_locator)
    axs[0, 1].xaxis.set_major_formatter(major_formatter)
    axs[0, 1].xaxis.set_minor_locator(minor_locator)
    axs[0, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 1].set_ylabel('hPa')
    axs[0, 1].set_title('pressure', fontsize=18)
    axs[0, 1].plot(df['pressure'].dropna(), 's-', label='pressure[hPa]')
    axs[0, 1].grid(which='both', axis='both')
    axs[0, 1].legend(fontsize=18)
    #
    axs[1, 0].xaxis.set_major_locator(major_locator)
    axs[1, 0].xaxis.set_major_formatter(major_formatter)
    axs[1, 0].xaxis.set_minor_locator(minor_locator)
    axs[1, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 0].set_ylabel('%RH')
    axs[1, 0].set_title('relative humidity', fontsize=18)
    axs[1, 0].plot(df['humidity'].dropna(), 's-',
                   label='relative humidity[%RH]')
    axs[1, 0].grid(which='both', axis='both')
    axs[1, 0].legend(fontsize=18)
    #
    axs[1, 1].xaxis.set_major_locator(major_locator)
    axs[1, 1].xaxis.set_major_formatter(major_formatter)
    axs[1, 1].xaxis.set_minor_locator(minor_locator)
    axs[1, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 1].set_ylabel('$mg/m^3$')
    axs[1, 1].set_title('absolute humidity', fontsize=18)
    axs[1, 1].plot(df['absolute_humidity'].dropna(), 's-',
                   label='absolute humidity[$g/m^3$]')
    axs[1, 1].grid(which='both', axis='both')
    axs[1, 1].legend(fontsize=18)
    #
    axs[2, 0].xaxis.set_major_locator(major_locator)
    axs[2, 0].xaxis.set_major_formatter(major_formatter)
    axs[2, 0].xaxis.set_minor_locator(minor_locator)
    axs[2, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 0].set_ylabel('ppm')
    axs[2, 0].set_title('equivalent CO2', fontsize=18)
    axs[2, 0].plot(df['eCo2'].dropna(), 's-', label='eCO2[ppm]')
    axs[2, 0].grid(which='both', axis='both')
    axs[2, 0].legend(fontsize=18)
    #
    axs[2, 1].xaxis.set_major_locator(major_locator)
    axs[2, 1].xaxis.set_major_formatter(major_formatter)
    axs[2, 1].xaxis.set_minor_locator(minor_locator)
    axs[2, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 1].set_ylabel('ppb')
    axs[2, 1].set_title('Total VOC', fontsize=18)
    axs[2, 1].plot(df['tvoc'].dropna(), 's-', label='TVOC[ppb]')
    axs[2, 1].grid(which='both', axis='both')
    axs[2, 1].legend(fontsize=18)
    #
    axs[3, 0].xaxis.set_major_locator(major_locator)
    axs[3, 0].xaxis.set_major_formatter(major_formatter)
    axs[3, 0].xaxis.set_minor_locator(minor_locator)
    axs[3, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[3, 0].set_yscale('log')
    axs[3, 0].set_ylabel('ppm')
    axs[3, 0].set_title('equivalent CO2', fontsize=18)
    axs[3, 0].plot(df['eCo2'].dropna(), 's-', label='eCO2[ppm]')
    axs[3, 0].grid(which='both', axis='both')
    axs[3, 0].legend(fontsize=18)
    #
    axs[3, 1].xaxis.set_major_locator(major_locator)
    axs[3, 1].xaxis.set_major_formatter(major_formatter)
    axs[3, 1].xaxis.set_minor_locator(minor_locator)
    axs[3, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[3, 1].set_yscale('log')
    axs[3, 1].set_ylabel('ppb')
    axs[3, 1].set_title('Total VOC', fontsize=18)
    axs[3, 1].plot(df['tvoc'].dropna(), 's-', label='TVOC[ppb]')
    axs[3, 1].grid(which='both', axis='both')
    axs[3, 1].legend(fontsize=18)
    #
#    fig.tight_layout()
    fig.savefig(filename)


def take_first_and_last_items(container):
    def do_query(query):
        targets = list(container.query_items(
            query=query,
            enable_cross_partition_query=True))
        if len(targets) > 0:
            items = list(container.query_items(
                query="SELECT * FROM c WHERE c.measuredAt=@at",
                parameters=[
                    {"name": "@at", "value": targets[0]}
                ],
                enable_cross_partition_query=True))
            return items[0]
        return None
    first = do_query("SELECT VALUE(MIN(c.measuredAt)) FROM c")
    last = do_query("SELECT VALUE(MAX(c.measuredAt)) FROM c")
    return (first, last)


def date_sequence(first, last):
    first_ = datetime.strptime(first.get("measuredAt"), "%Y-%m-%dT%H:%M:%SZ")
    last_ = datetime.strptime(last.get("measuredAt"), "%Y-%m-%dT%H:%M:%SZ")
    t = first_
    while t <= last_:
        yield t
        t += timedelta(days=1)


def split_weekly(first_last_items):
    def attach_weeknumber(ds):
        return [(d.isocalendar()[1], d) for d in ds]

    def key(keyvalue):
        (k, v) = keyvalue
        return k

    def value(keyvalue):
        (k, v) = keyvalue
        return v
    ds = date_sequence(*first_last_items)
    xs = attach_weeknumber(ds)
    xxs = itertools.groupby(xs, key)
    for _, group in xxs:
        yield [value(kv) for kv in group]


def run(url, key):
    cosmos_client = CosmosClient(url, credential=key)
    database_name = "ThingsDatabase"
    container_name = "Measurements"
    database = cosmos_client.get_database_client(database_name)
    container = database.get_container_client(container_name)
    first_last_items = take_first_and_last_items(container)
    weeklies = split_weekly(first_last_items)
    for week in weeklies:
        begin = week[0]
        end = week[-1] + timedelta(days=1) - timedelta(microseconds=1)
        df = take_items_from_container(container, begin, end)
        begin_ = begin.strftime('%Y-%m-%d')
        end_ = end.strftime('%Y-%m-%d')
        filename = "{}_{}.png".format(begin_, end_)
        plot(df, filename)
        print("----------")


if __name__ == "__main__":
    if len(sys.argv) <= 2:
        print("{} url key".format(sys.argv[0]))
    else:
        url = sys.argv[1]
        key = sys.argv[2]
        run(url, key)
