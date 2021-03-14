#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos
#
# Copyright (c) 2021 Akihiro Yamamoto.
# Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
# See LICENSE file in the project root for full license information.

from azure.cosmos import CosmosClient
import sys
from pytz import timezone
from datetime import datetime
from dateutil import parser


def list_temp_humi_pres_tvoc_co2(item_list):
    for doc in item_list:
        id_ = doc.get('id')
        sid = doc.get('sensorId')
        mid = doc.get('messageId')
        at = doc.get('measuredAt')
        at_asia_tokyo = parser.parse(at).astimezone(timezone('Asia/Tokyo'))
        temp = doc.get('temperature')
        humi = doc.get('humidity')
        pres = doc.get('pressure')
        tvoc = doc.get('tvoc')
        tvoc_base = doc.get('tvoc_baseline')
        eCo2 = doc.get('eCo2')
        eCo2_base = doc.get('eCo2_baseline')
        co2 = doc.get('co2')
        #
        output = '{} {:30}'.format(id_, sid)
        output += ' {:6} "{}" "{}"'.format(mid, at, at_asia_tokyo)
        output += ' {:5.1f}C'.format(temp) if temp is not None else "  NaN C"
        output += ' {:4.1f}%'.format(humi) if humi is not None else " NaN %"
        output += ' {:6.1f}hPa'.format(
            pres) if pres is not None else "   NaN hPa"
        output += ' {:5}ppb'.format(tvoc) if tvoc is not None else "  NaN ppb"
        output += ' {:5}ppm'.format(eCo2) if eCo2 is not None else "  NaN ppm"
        output += ' {:5}tvoc_base'.format(
            tvoc_base) if tvoc_base is not None else "  NaN tvoc_base"
        output += ' {:5}eCo2_base'.format(
            eCo2_base) if eCo2_base is not None else "  NaN eCo2_base"
        output += ' {:5}ppm'.format(co2) if co2 is not None else "  NaN ppm"
        print(output)


def list_all_temp_humi_pres_tvoc_co2(container):
    item_list = list(container.read_all_items(max_item_count=100))
    print('Found {} items'.format(item_list.__len__()))
    list_temp_humi_pres_tvoc_co2(item_list)


if __name__ == "__main__":
    if len(sys.argv) <= 2:
        print("{} url key".format(sys.argv[0]))
    else:
        url = sys.argv[1]
        key = sys.argv[2]
        client = CosmosClient(url, credential=key)
        database_name = "ThingsDatabase"
        container_name = "Measurements"
        database = client.get_database_client(database_name)
        container = database.get_container_client(container_name)
        list_all_temp_humi_pres_tvoc_co2(container)
