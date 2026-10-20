'''
Air Quality sensor driver launch file

Luca Callegari <luketto.c@gmail.com>
Mirko Damiano <mirketto2001.md@gmail.com>

August 4, 2026
'''

import os
from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch_ros.actions import Node
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

def generate_launch_description():
  ld = LaunchDescription()

  container = ComposableNodeContainer(
    name='server_container',
    namespace='server_components',
    package='rclcpp_components',
    executable='component_container',
    emulate_tty=True,
    output='both',
    log_cmd=True,
    composable_node_descriptions=[
        ComposableNode(
            package='server_raw',
            plugin='server_components::ReaderNode',
            name='reader_node',
            namespace='server_components',
            parameters=[],
            extra_arguments=[{'use_intra_process_comms': True}])
    ]
  )

  ld.add_action(container)
  return ld
