# Copyright (c) 2021-2023, NVIDIA CORPORATION.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import importlib
import typing

import morpheus.service  # pylint: disable=unused-import


class VectorDBServiceFactory:

    @typing.overload
    @classmethod
    def create_instance(cls, service_name: typing.Literal["milvus"], *args: typing.Any,
                        **kwargs) -> "morpheus.service.milvus_vector_db_service.MilvusVectorDBService":
        pass

    @classmethod
    def create_instance(cls, service_name: str, *args: typing.Any, **kwargs):
        """
        Factory for creating instances of vector database service classes. This factory allows dynamically
        creating instances of vector database service classes based on the provided service name.
        Each service name corresponds to a specific implementation class.

        Parameters
        ----------
        service_name : str
            The name of the vector database service to create.
        *args : typing.Any
            Variable-length argument list to pass to the service constructor.
        **kwargs : dict[str, typing.Any]
            Arbitrary keyword arguments to pass to the service constructor.

        Returns
        -------
            An instance of the specified vector database service class.

        Raises
        ------
        ValueError
            If the specified service name is not found or does not correspond to a valid service class.
        """

        module_name = f"morpheus.service.{service_name}_vector_db_service"

        try:
            module = importlib.import_module(module_name)
            class_name = f"{service_name.capitalize()}VectorDBService"
            class_ = getattr(module, class_name)
            instance = class_(*args, **kwargs)
            return instance
        except (ModuleNotFoundError, AttributeError) as exc:
            raise ValueError(f"Service {service_name} not found. Ensure that the corresponding service class," +
                             f"such as {module_name}, has been implemented.") from exc
