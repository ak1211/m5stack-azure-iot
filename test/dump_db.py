#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
from pytz import timezone
from datetime import datetime
from dateutil import parser


def list_all_temp_humi_pres(container):
    item_list = list(container.read_all_items())
    print('Found {} items'.format(item_list.__len__()))
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
        #
        output = '{} {:30}'.format(id_, sid)
        output += ' {:6} "{}" "{}"'.format(mid, at, at_asia_tokyo)
        output += ' {:5.1f}C'.format(temp) if temp is not None else "  NaN C"
        output += ' {:4.1f}%'.format(humi) if humi is not None else " NaN %"
        output += ' {:6.1f}hPa'.format(
            pres) if pres is not None else "   NaN hPa"
        output += ' {:5}ppb({:4})'.format(tvoc,
                                          tvoc_base) if tvoc is not None else "  NaN ppb(  NaN )"
        output += ' {:5}ppm({:4})'.format(eCo2,
                                          eCo2_base) if eCo2 is not None else "  NaN ppm(  NaN )"
        print(output)


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
        list_all_temp_humi_pres(container)
