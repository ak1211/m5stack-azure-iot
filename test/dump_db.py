#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys


def list_all_temp_humi_pres(container):
    item_list = list(container.read_all_items())
    print('Found {} items'.format(item_list.__len__))
    for doc in item_list:
        id_ = doc.get('messageId')
        at = doc.get('measuredAt')
        t = doc.get('temperature')
        h = doc.get('humidity')
        p = doc.get('pressure')
        print('{0} {1}  {2:4.1f}C  {3:4.1f}%  {4:6.1f}hPa'.format(id_, at, t, h, p))


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
