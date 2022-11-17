#!/usr/bin/env python3
#
# データーベースに入っている余計なアイテムを削除する
#
# $ pip3 install numpy pandas matplotlib azure-cosmos
# $ ./plot_db.py azure-cosmos-db-uri azure-cosmos-db-key
#
# Copyright (c) 2022 Akihiro Yamamoto.
# Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
# See LICENSE file in the project root for full license information.

from azure.cosmos import CosmosClient, exceptions
import sys
import pprint


def take_sensorId(container):
    results = list(container.query_items(
        query="SELECT DISTINCT c.sensorId FROM c",
        enable_cross_partition_query=True))
    return results


def take_items_from_container(container, sensorId):
    item_list = list(container.query_items(
        query="SELECT c.id,c.sensorId FROM c WHERE c.sensorId=@sid",
        parameters=[
            {"name": "@sid", "value": sensorId},
        ],
        enable_cross_partition_query=True))
    return item_list


def run(url, key):
    cosmos_client = CosmosClient(url, credential=key)
    database_name = "ThingsDatabase"
    container_name = "Measurements"
    database = cosmos_client.get_database_client(database_name)
    container = database.get_container_client(container_name)
    #
    #
    # 'm5stack-bme280-device-bme280'
    # 'm5stack-bme280-device-sgp30'
    # 'm5stack-bme280-device-scd30'
    # 上の３つ以外の"sensorId"のアイテムを削除する
    a = take_sensorId(container)
    b = [x for x in a if x['sensorId'] != 'm5stack-bme280-device-bme280']
    c = [x for x in b if x['sensorId'] != 'm5stack-bme280-device-sgp30']
    d = [x for x in c if x['sensorId'] != 'm5stack-bme280-device-scd30']
    sensorIds = d
    #
    # 削除対象のsensorIdを持つレコードのidを取得する
    #
    delete_id_list = []
    for sid in sensorIds:
        items = take_items_from_container(container, sid.get('sensorId'))
        delete_id_list.extend(items)
    pprint.pprint(delete_id_list)
    #
    # 削除対象idのレコードを削除する
    #
    for delete_item_id in delete_id_list:
        id = delete_item_id.get('id')
        pkey = delete_item_id.get('sensorId')
        print("Delete item id={}".format(id))
        try:
            container.delete_item(item=id, partition_key=pkey)
        except exceptions.CosmosResourceNotFoundError:
            pass


if __name__ == "__main__":
    if len(sys.argv) <= 2:
        print("{} url key".format(sys.argv[0]))
    else:
        url = sys.argv[1]
        key = sys.argv[2]
        run(url, key)
