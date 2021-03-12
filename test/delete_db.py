#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient, exceptions
import sys
from pytz import timezone
from datetime import datetime
from dateutil import parser

delete_list = [
    ("0e13b3e6-ce53-4153-a4cf-4b83169211d1", "m5stack-bme280-device-scd30"),
    ("aa23e329-d110-46c9-99fc-b16733c98c21", "m5stack-bme280-device-scd30"),
    ("3861154e-af83-48c6-aa0e-d3ae8dcae3ad", "m5stack-bme280-device-scd30"),
    ("3ae2b89a-1575-4e13-8196-1ece87d5086f", "m5stack-bme280-device-scd30"),
    ("91b08f66-9d9d-497e-9bae-b0e7b3d97930", "m5stack-bme280-device-scd30"),
    ("31df459d-a0f3-4f96-8997-a875420f18c9", "m5stack-bme280-device-scd30"),
    ("11356262-900b-4013-98d6-3e8d7c6cb56c", "m5stack-bme280-device-scd30"),
    ("c9ec9faa-9fac-4a53-8ef1-e07893a86aeb", "m5stack-bme280-device-scd30"),
    ("cb6f43fa-d91b-4fa6-ae18-938498a9b3f5", "m5stack-bme280-device-scd30"),
    ("85b2b4ac-f156-42a1-b25b-5ab018e56c16", "m5stack-bme280-device-scd30"),
    ("b042af2c-ff71-4de0-903c-a525913f0abe", "m5stack-bme280-device-scd30"),
    ("df7ba1f8-ead2-4547-9d7d-48166233bc19", "m5stack-bme280-device-scd30"),
    ("c7e97929-31d3-421b-b9ff-96891ecf174d", "m5stack-bme280-device-scd30"),
    ("14d0f7a5-fca5-4805-8898-e07b56643c35", "m5stack-bme280-device-scd30"),
]

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
        #
        for (delete_item_id, sensor_id) in delete_list:
            print("Delete item id={}".format(delete_item_id))
            try:
                container.delete_item(
                    item=delete_item_id, partition_key=sensor_id)
            except exceptions.CosmosResourceNotFoundError:
                pass
