#!/usr/bin/env python3
#
# $ pip3 install azure-cosmos

from azure.cosmos import CosmosClient
import sys
from pytz import timezone
from datetime import datetime
from dateutil import parser


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
        delete_item_id = "c73d0103-5ddf-4bc2-8297-5da313c59be1"
        sensor_id = "m5stack-bme280-device-sgp30"
        #
        print("Delete item id={}\n".format(delete_item_id))
        container.delete_item(item=delete_item_id, partition_key=sensor_id)
