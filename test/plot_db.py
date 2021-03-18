#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos
#
# Copyright (c) 2021 Akihiro Yamamoto.
# Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
# See LICENSE file in the project root for full license information.

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


def time_sequential_data_frame(item_list, tz):
    pairs = [('sensorId', lambda x:x),
             ('measuredAt', lambda x:parser.parse(x).astimezone(tz)),
             ('temperature', lambda x: float(x) if x is not None else None),
             ('humidity', lambda x: float(x) if x is not None else None),
             ('pressure', lambda x: float(x) if x is not None else None),
             ('tvoc', lambda x: int(x) if x is not None else None),
             ('eCo2', lambda x: int(x) if x is not None else None),
             ('co2', lambda x: int(x) if x is not None else None)]
    columns, _ = zip(*pairs)

    def pickup(item):
        return [func(item.get(col)) for (col, func) in pairs]
    #
    data = [pickup(item) for item in item_list]
    df = pd.DataFrame(data=data, columns=columns)
    return df


def take_all_items_from_container(container, tz):
    item_list = list(container.read_all_items())
    return time_sequential_data_frame(item_list, tz)


def take_items_from_container(container, begin, end, tz):
    begin_ = begin.astimezone(timezone('UTC')).isoformat()
    end_ = end.astimezone(timezone('UTC')).isoformat()
    print("{} -> {}".format(begin, end))
    item_list = list(container.query_items(
        query="SELECT * FROM c WHERE c.measuredAt BETWEEN @begin AND @end",
        parameters=[
            {"name": "@begin", "value": begin_},
            {"name": "@end", "value": end_}
        ],
        enable_cross_partition_query=True))
    return time_sequential_data_frame(item_list, tz)


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


def plot(df, sensorIds, filename, tz):
    #
    df = calculate_absolute_humidity(df)
    df = df.set_index('measuredAt')
    print(df)
    #
    major_formatter = DateFormatter('%a\n%Y-%m-%d\n%H:%M:%S\n%Z', tz=tz)
    major_locator = DayLocator(tz=tz)
    minor_formatter = DateFormatter('%H', tz=tz)
    minor_locator = HourLocator(byhour=range(0, 24, 6), tz=tz)
    #
    xlim = [df.index[0].astimezone(tz).replace(hour=0, minute=0, second=0, microsecond=0),
            df.index[-1]]
    #
    fig, axs = plt.subplots(4, 2, figsize=(32, 32))
    #
    axs[0, 0].xaxis.set_major_locator(major_locator)
    axs[0, 0].xaxis.set_major_formatter(major_formatter)
    axs[0, 0].xaxis.set_minor_locator(minor_locator)
    axs[0, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 0].set_xlim(xlim)
    axs[0, 0].set_ylim(-10.0, 60.0)
    axs[0, 0].set_ylabel('$^{\circ}C$')
    axs[0, 0].set_title('temperature', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('temperature').dropna()
        if len(ddf) > 0:
            axs[0, 0].plot(ddf, 'o-', label=sid)
    axs[0, 0].grid(which='both', axis='both')
    axs[0, 0].legend(fontsize=18)
    #
    axs[0, 1].xaxis.set_major_locator(major_locator)
    axs[0, 1].xaxis.set_major_formatter(major_formatter)
    axs[0, 1].xaxis.set_minor_locator(minor_locator)
    axs[0, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[0, 1].set_xlim(xlim)
    axs[0, 1].set_ylim(940.0, 1060.0)
    axs[0, 1].set_ylabel('hPa')
    axs[0, 1].set_title('pressure', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('pressure').dropna()
        if len(ddf) > 0:
            axs[0, 1].plot(ddf, 's-', label=sid)
    axs[0, 1].grid(which='both', axis='both')
    axs[0, 1].legend(fontsize=18)
    #
    axs[1, 0].xaxis.set_major_locator(major_locator)
    axs[1, 0].xaxis.set_major_formatter(major_formatter)
    axs[1, 0].xaxis.set_minor_locator(minor_locator)
    axs[1, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 0].set_xlim(xlim)
    axs[1, 0].set_ylim(0.0, 100.0)
    axs[1, 0].set_ylabel('%RH')
    axs[1, 0].set_title('relative humidity', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('humidity').dropna()
        if len(ddf) > 0:
            axs[1, 0].plot(ddf, 's-', label=sid)
    axs[1, 0].grid(which='both', axis='both')
    axs[1, 0].legend(fontsize=18)
    #
    axs[1, 1].xaxis.set_major_locator(major_locator)
    axs[1, 1].xaxis.set_major_formatter(major_formatter)
    axs[1, 1].xaxis.set_minor_locator(minor_locator)
    axs[1, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[1, 1].set_xlim(xlim)
    axs[1, 1].set_ylabel('$mg/m^3$')
    axs[1, 1].set_title('absolute humidity', fontsize=18)
    axs[1, 1].plot(df['absolute_humidity'].dropna(), 's-')
    axs[1, 1].grid(which='both', axis='both')
#    axs[1, 1].legend(fontsize=18)
    #
    axs[2, 0].xaxis.set_major_locator(major_locator)
    axs[2, 0].xaxis.set_major_formatter(major_formatter)
    axs[2, 0].xaxis.set_minor_locator(minor_locator)
    axs[2, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 0].set_xlim(xlim)
    axs[2, 0].set_ylabel('ppm')
    axs[2, 0].set_title('(equivalent) CO2', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('eCo2').dropna()
        if len(ddf) > 0:
            axs[2, 0].plot(ddf, 's-', label=sid)
        else:
            ddf = df.query(qry_str).get('co2').dropna()
            if len(ddf) > 0:
                axs[2, 0].plot(ddf, 's-', label=sid)

    axs[2, 0].grid(which='both', axis='both')
    axs[2, 0].legend(fontsize=18)
    #
    axs[2, 1].xaxis.set_major_locator(major_locator)
    axs[2, 1].xaxis.set_major_formatter(major_formatter)
    axs[2, 1].xaxis.set_minor_locator(minor_locator)
    axs[2, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[2, 1].set_xlim(xlim)
    axs[2, 1].set_ylabel('ppb')
    axs[2, 1].set_title('Total VOC', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('tvoc').dropna()
        if len(ddf) > 0:
            axs[2, 1].plot(ddf, 's-', label=sid)
    axs[2, 1].grid(which='both', axis='both')
    axs[2, 1].legend(fontsize=18)
    #
    axs[3, 0].xaxis.set_major_locator(major_locator)
    axs[3, 0].xaxis.set_major_formatter(major_formatter)
    axs[3, 0].xaxis.set_minor_locator(minor_locator)
    axs[3, 0].xaxis.set_minor_formatter(minor_formatter)
    axs[3, 0].set_xlim(xlim)
    axs[3, 0].set_yscale('log')
    axs[3, 0].set_ylabel('ppm')
    axs[3, 0].set_title('(equivalent) CO2', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('eCo2').dropna()
        if len(ddf) > 0:
            axs[3, 0].plot(ddf, 's-', label=sid)
        else:
            ddf = df.query(qry_str).get('co2').dropna()
            if len(ddf) > 0:
                axs[3, 0].plot(ddf, 's-', label=sid)
    axs[3, 0].grid(which='both', axis='both')
    axs[3, 0].legend(fontsize=18)
    #
    axs[3, 1].xaxis.set_major_locator(major_locator)
    axs[3, 1].xaxis.set_major_formatter(major_formatter)
    axs[3, 1].xaxis.set_minor_locator(minor_locator)
    axs[3, 1].xaxis.set_minor_formatter(minor_formatter)
    axs[3, 1].set_xlim(xlim)
    axs[3, 1].set_yscale('log')
    axs[3, 1].set_ylabel('ppb')
    axs[3, 1].set_title('Total VOC', fontsize=18)
    for sid in sensorIds:
        qry_str = "sensorId=='{}'".format(sid)
        ddf = df.query(qry_str).get('tvoc').dropna()
        if len(ddf) > 0:
            axs[3, 1].plot(ddf, 's-', label=sid)
    axs[3, 1].grid(which='both', axis='both')
    axs[3, 1].legend(fontsize=18)
    #
#    fig.tight_layout()
    fig.savefig(filename)


def take_sensorId(container):
    results = list(container.query_items(
        query="SELECT DISTINCT c.sensorId FROM c",
        enable_cross_partition_query=True))
    return [x.get('sensorId') for x in results]


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


def date_sequence(begin, end):
    t = begin
    while t <= end:
        yield t
        t += timedelta(days=1)


def split_weekly(begin, end):
    def attach_weeknumber(ds):
        return [(d.isocalendar()[1], d) for d in ds]

    def key(keyvalue):
        (k, v) = keyvalue
        return k

    def value(keyvalue):
        (k, v) = keyvalue
        return v
    ds = date_sequence(begin, end)
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
    #
    tz = timezone('Asia/Tokyo')
    sensorIds = take_sensorId(container)
    (first_item, last_item) = take_first_and_last_items(container)
    #
    first = datetime.strptime(
        first_item.get("measuredAt"), "%Y-%m-%dT%H:%M:%SZ")
    first = first.astimezone(tz).replace(
        hour=0, minute=0, second=0, microsecond=0)
    #
    last = datetime.strptime(
        last_item.get("measuredAt"), "%Y-%m-%dT%H:%M:%SZ")
    last = last.astimezone(tz).replace(
        hour=23, minute=59, second=59, microsecond=999999)

    weeklies = split_weekly(first, last)
    for week in weeklies:
        begin = week[0]
        end = week[-1] + timedelta(days=1) - timedelta(microseconds=1)
        df = take_items_from_container(container, begin, end, tz)
        begin_ = begin.strftime('%Y-%m-%d')
        end_ = end.strftime('%Y-%m-%d')
        filename = "{}_{}.png".format(begin_, end_)
        plot(df, sensorIds, filename, tz)
        print("----------")


if __name__ == "__main__":
    if len(sys.argv) <= 2:
        print("{} url key".format(sys.argv[0]))
    else:
        url = sys.argv[1]
        key = sys.argv[2]
        run(url, key)
