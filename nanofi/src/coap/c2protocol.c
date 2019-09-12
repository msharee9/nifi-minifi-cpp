/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "cbor.h"
#include "coap/c2protocol.h"
#include "coap/c2agent.h"
#include "utlist.h"

#include <string.h>

void free_agent_manifest(c2heartbeat_t * hb) {
    if (!hb) return;

    int i;
    for (i = 0; i < hb->ag_manifest.num_ecus; ++i) {
        free((void *)hb->ag_manifest.ecus[i].input);
        free((void *)hb->ag_manifest.ecus[i].output);
        free((void *)hb->ag_manifest.ecus[i].name);
        free_properties(hb->ag_manifest.ecus[i].ip_args);
        free_properties(hb->ag_manifest.ecus[i].op_args);
    }
    free(hb->ag_manifest.ecus);

    for (i = 0; i < hb->ag_manifest.io.num_ips; ++i) {
        free(hb->ag_manifest.io.input_params[i].name);
        int n;
        for (n = 0; n < hb->ag_manifest.io.input_params[i].num_params; ++n) {
            free(hb->ag_manifest.io.input_params[i].params[n]);
        }
        free(hb->ag_manifest.io.input_params[i].params);
    }
    free(hb->ag_manifest.io.input_params);

    for (i = 0; i < hb->ag_manifest.io.num_ops; ++i) {
        free(hb->ag_manifest.io.output_params[i].name);
        int n;
        for (n = 0; n < hb->ag_manifest.io.output_params[i].num_params; ++n) {
            free(hb->ag_manifest.io.output_params[i].params[n]);
        }
        free(hb->ag_manifest.io.output_params[i].params);
    }
    free(hb->ag_manifest.io.output_params);
}

void free_c2heartbeat(c2heartbeat_t * c2_heartbeat) {
    if (!c2_heartbeat) {
        return;
    }
    char * ac = c2_heartbeat->agent_info.agent_class;
    if (ac) {
        free(ac);
    }
    c2_heartbeat->agent_info.agent_class = NULL;

    char * id = c2_heartbeat->device_info.ident;
    if (id) {
        free(id);
    }
    c2_heartbeat->device_info.ident = NULL;

    char * devid = c2_heartbeat->device_info.network_info.device_id;
    if (devid) {
        free(devid);
    }
    c2_heartbeat->device_info.network_info.device_id = NULL;
    if (c2_heartbeat->has_ag_manifest) {
        free_agent_manifest(c2_heartbeat);
    }
}

struct encoded_data encode_deviceinfo(const struct deviceinfo * dev_info) {
    struct encoded_data encoded_devinfo;
    memset(&encoded_devinfo, 0, sizeof(encoded_devinfo));

    cbor_item_t * type = cbor_build_uint8(1);
    cbor_item_t * id = cbor_build_string(dev_info->ident);
    cbor_item_t * has_system_info = cbor_build_uint8(1);
    cbor_item_t * machine_arch = cbor_build_string(dev_info->system_info.machine_arch);
    cbor_item_t * num_vcores = cbor_build_uint16(dev_info->system_info.v_cores);
    cbor_item_t * physical_mem = cbor_build_uint64(dev_info->system_info.physical_mem);
    cbor_item_t * has_network_info = cbor_build_uint8(1);
    cbor_item_t * device_id = cbor_build_string(dev_info->network_info.device_id);
    cbor_item_t * host_name = cbor_build_string(dev_info->network_info.host_name);
    cbor_item_t * ip_addr = cbor_build_string(dev_info->network_info.ip_address);

    size_t type_bytes = (1 + 1);
    size_t id_bytes = (cbor_string_length(id) + 8 + 1);
    size_t sys_info_bytes = (1 + 1);
    size_t machine_arch_bytes = cbor_string_length(machine_arch) + 8 + 1;
    size_t num_vcores_bytes = 2 + 1;
    size_t phys_mem_bytes = 8 + 1;
    size_t net_info_bytes = 1 + 1;
    size_t device_id_bytes = cbor_string_length(device_id) + 8 + 1;
    size_t host_name_bytes = cbor_string_length(host_name) + 8 + 1;
    size_t ip_addr_bytes = cbor_string_length(ip_addr) + 8 + 1;

    unsigned char * buffer = (unsigned char *)malloc(type_bytes
                                                    + id_bytes
                                                    + sys_info_bytes
                                                    + machine_arch_bytes
                                                    + num_vcores_bytes
                                                    + phys_mem_bytes
                                                    + net_info_bytes
                                                    + device_id_bytes
                                                    + host_name_bytes
                                                    + ip_addr_bytes);

    size_t written = 0;
    written += cbor_serialize_uint(type, buffer + written, type_bytes);
    written += cbor_serialize_string(id, buffer + written, id_bytes);
    written += cbor_serialize_uint(has_system_info, buffer + written, sys_info_bytes);
    written += cbor_serialize_string(machine_arch, buffer + written, machine_arch_bytes);
    written += cbor_serialize_uint(num_vcores, buffer + written, num_vcores_bytes);
    written += cbor_serialize_uint(physical_mem, buffer + written, phys_mem_bytes);
    written += cbor_serialize_uint(has_network_info, buffer + written, net_info_bytes);
    written += cbor_serialize_string(device_id, buffer + written, device_id_bytes);
    written += cbor_serialize_string(host_name, buffer + written, host_name_bytes);
    written += cbor_serialize_string(ip_addr, buffer + written, ip_addr_bytes);
    cbor_decref(&type);
    cbor_decref(&id);
    cbor_decref(&has_system_info);
    cbor_decref(&machine_arch);
    cbor_decref(&num_vcores);
    cbor_decref(&physical_mem);
    cbor_decref(&has_network_info);
    cbor_decref(&device_id);
    cbor_decref(&host_name);
    cbor_decref(&ip_addr);

    encoded_devinfo.buff = buffer;
    encoded_devinfo.length = written;
    return encoded_devinfo;
}

struct encoded_data encode_agentinfo(const struct agentinfo * agent_info) {
    struct encoded_data encoded_agentinfo;
    memset(&encoded_agentinfo, 0, sizeof(encoded_agentinfo));

    cbor_item_t * agent_info_id = cbor_build_string(agent_info->ident);
    cbor_item_t * agent_class = cbor_build_string(agent_info->agent_class);
    cbor_item_t * uptime = cbor_build_uint64(agent_info->uptime);

    size_t agent_info_bytes = (cbor_string_length(agent_info_id) + 8 + 1);
    size_t agent_class_bytes = (cbor_string_length(agent_class) + 8 + 1);
    size_t uptime_bytes = (8 + 1);

    unsigned char * buffer = (unsigned char *)malloc(agent_info_bytes + agent_class_bytes + uptime_bytes);

    size_t written = 0;
    written += cbor_serialize_string(agent_info_id, buffer + written, agent_info_bytes);
    written += cbor_serialize_string(agent_class, buffer + written, agent_class_bytes);
    written += cbor_serialize_uint(uptime, buffer + written, uptime_bytes);
    cbor_decref(&agent_info_id);
    cbor_decref(&agent_class);
    cbor_decref(&uptime);

    encoded_agentinfo.buff = buffer;
    encoded_agentinfo.length = written;
    return encoded_agentinfo;
}

struct encoded_data encode_ecuinfo(const ecuinfo * ecu_info) {
    struct encoded_data encoded_ecuinfo;
    memset(&encoded_ecuinfo, 0, sizeof(encoded_ecuinfo));
    if (!ecu_info) return encoded_ecuinfo;

    cbor_item_t * uuid_item = cbor_build_string(ecu_info->uuid);
    cbor_item_t * name_item = NULL;
    if (ecu_info->name) {
        name_item = cbor_build_string(ecu_info->name);
    }

    cbor_item_t * input_item = NULL;
    if (ecu_info->input) {
        input_item = cbor_build_string(ecu_info->input);
    }

    cbor_item_t * output_item = NULL;
    if (ecu_info->output) {
        output_item = cbor_build_string(ecu_info->output);
    }

    size_t num_ip_params = HASH_COUNT(ecu_info->ip_args);
    size_t num_op_params = HASH_COUNT(ecu_info->op_args);
    cbor_item_t * ip_args_map = cbor_new_definite_map(num_ip_params);
    cbor_item_t * op_args_map = cbor_new_definite_map(num_op_params);

    properties_t * el, * tmp;
    HASH_ITER(hh, ecu_info->ip_args, el, tmp) {
        cbor_map_add(ip_args_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_string(el->key)),
                         .value = cbor_move(cbor_build_string(el->value))});
    }

    HASH_ITER(hh, ecu_info->op_args, el, tmp) {
        cbor_map_add(op_args_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_string(el->key)),
                         .value = cbor_move(cbor_build_string(el->value))});
    }

    unsigned char * ip_map_buff;
    size_t ip_map_buff_size;
    size_t ip_args_bytes = cbor_serialize_alloc(ip_args_map, &ip_map_buff, &ip_map_buff_size);
    cbor_decref(&ip_args_map);

    unsigned char * op_map_buff;
    size_t op_map_buff_size;
    size_t op_args_bytes = cbor_serialize_alloc(op_args_map, &op_map_buff, &op_map_buff_size);
    cbor_decref(&op_args_map);

    size_t uuid_bytes = cbor_string_length(uuid_item) + 8 + 1;
    size_t name_bytes = cbor_string_length(name_item) + 8 + 1;
    size_t input_bytes = cbor_string_length(input_item) + 8 + 1;
    size_t output_bytes = cbor_string_length(output_item) + 8 + 1;

    unsigned char * buffer = (unsigned char *)malloc(uuid_bytes + name_bytes + input_bytes + ip_args_bytes + output_bytes + op_args_bytes);
    size_t written = 0;
    written += cbor_serialize_string(uuid_item, buffer + written, uuid_bytes);
    written += cbor_serialize_string(name_item, buffer + written, name_bytes);
    written += cbor_serialize_string(input_item, buffer + written, input_bytes);
    memcpy(buffer + written, (char *)ip_map_buff, ip_args_bytes);
    written += ip_args_bytes;
    written += cbor_serialize_string(output_item, buffer + written, output_bytes);
    memcpy(buffer + written, (char *)op_map_buff, op_args_bytes);
    written += op_args_bytes;

    free(ip_map_buff);
    free(op_map_buff);
    cbor_decref(&uuid_item);
    cbor_decref(&name_item);
    cbor_decref(&input_item);
    cbor_decref(&output_item);

    encoded_ecuinfo.buff = buffer;
    encoded_ecuinfo.length = written;
    return encoded_ecuinfo;
}

typedef struct {
    cbor_item_t * item;
    size_t bytes;
} cbor_item_info;

struct encoded_data encode_io_manifest(const io_manifest * io_mnfst) {
    struct encoded_data encoded_io;
    memset(&encoded_io, 0, sizeof(struct encoded_data));

    cbor_item_t * num_ips = cbor_build_uint8((uint8_t)io_mnfst->num_ips);

    size_t num_ips_bytes = 1 + 1;

    size_t total_len = 1024;
    size_t written = 0;

    unsigned char * buffer = (unsigned char *)malloc(total_len);
    written += cbor_serialize_uint(num_ips, buffer + written, num_ips_bytes);
    cbor_decref(&num_ips);

    int i;
    for (i = 0; i < io_mnfst->num_ips; ++i) {
        size_t sub_length = 0;
        cbor_item_t * name_item = cbor_build_string(io_mnfst->input_params[i].name);
        size_t name_bytes = cbor_string_length(name_item) + 8 + 1;
        cbor_item_t * num_params_item = cbor_build_uint16(io_mnfst->input_params[i].num_params);
        size_t num_params_bytes = 2 + 1;

        sub_length = name_bytes + num_params_bytes;
        if (sub_length > (total_len - written)) {
            //we need reallocation of buffer into a larger buffer size
            reallocate_buffer(&buffer, sub_length, written, &total_len);
        }
        written += cbor_serialize_string(name_item, buffer + written, name_bytes);
        written += cbor_serialize_uint(num_params_item, buffer + written, num_params_bytes);
        cbor_decref(&name_item);
        cbor_decref(&num_params_item);

        int n;
        for (n = 0; n < io_mnfst->input_params[i].num_params; ++n) {
            cbor_item_t * param_item = cbor_build_string(io_mnfst->input_params[i].params[n]);
            size_t param_length = cbor_string_length(param_item) + 8 + 1;
            if (param_length > (total_len - written)) {
                //we need reallocation of buffer into a larger buffer size
                reallocate_buffer(&buffer, param_length, written, &total_len);
            }
            written += cbor_serialize_string(param_item, buffer + written, param_length);
            cbor_decref(&param_item);
    	}
    }

    cbor_item_t * num_ops = cbor_build_uint8((uint8_t)io_mnfst->num_ops);
    size_t num_ops_bytes = 1 + 1;

    if (num_ops_bytes > (total_len - written)) {
    	reallocate_buffer(&buffer, num_ops_bytes, written, &total_len);
    }
    written += cbor_serialize_uint(num_ops, buffer + written, num_ops_bytes);
    cbor_decref(&num_ops);

    for (i = 0; i < io_mnfst->num_ops; ++i) {
        size_t sub_length = 0;
        cbor_item_t * name_item = cbor_build_string(io_mnfst->output_params[i].name);
        size_t name_bytes = cbor_string_length(name_item) + 8 + 1;
        cbor_item_t * num_params_item = cbor_build_uint16(io_mnfst->output_params[i].num_params);
        size_t num_params_bytes = 2 + 1;

        sub_length = name_bytes + num_params_bytes;
        if (sub_length > (total_len - written)) {
            //we need reallocation of buffer into a larger buffer size
            reallocate_buffer(&buffer, sub_length, written, &total_len);
        }
        written += cbor_serialize_string(name_item, buffer + written, name_bytes);
        written += cbor_serialize_uint(num_params_item, buffer + written, num_params_bytes);
        cbor_decref(&name_item);
        cbor_decref(&num_params_item);

        int n;
        for (n = 0; n < io_mnfst->output_params[i].num_params; ++n) {
            cbor_item_t * param_item = cbor_build_string(io_mnfst->output_params[i].params[n]);
            size_t param_length = cbor_string_length(param_item) + 8 + 1;
            if (param_length > (total_len - written)) {
                //we need reallocation of buffer into a larger buffer size
                reallocate_buffer(&buffer, param_length, written, &total_len);
            }
            written += cbor_serialize_string(param_item, buffer + written, param_length);
            cbor_decref(&param_item);
    	}
    }

    encoded_io.buff = buffer;
    encoded_io.length = written;
    return encoded_io;
}

struct encoded_data encode_agent_manifest(const agent_manifest * ag_manifest) {
    struct encoded_data encoded_agentmnfst;
    memset(&encoded_agentmnfst, 0, sizeof(encoded_agentmnfst));

    cbor_item_t * num_ecus = cbor_build_uint64(ag_manifest->num_ecus);

    size_t num_bytes_ecuinfo = 0;
    struct encoded_data * encoded_ecuinfos = NULL;
    if (ag_manifest->num_ecus) {
        encoded_ecuinfos = (struct encoded_data *)malloc(sizeof(struct encoded_data) * ag_manifest->num_ecus);
        memset(encoded_ecuinfos, 0, sizeof(struct encoded_data) * ag_manifest->num_ecus);

        int i;
        for (i = 0; i < ag_manifest->num_ecus; ++i) {
            struct encoded_data encoded_ecuinfo = encode_ecuinfo(&ag_manifest->ecus[i]);
            encoded_ecuinfos[i].buff = encoded_ecuinfo.buff;
            encoded_ecuinfos[i].length = encoded_ecuinfo.length;
            num_bytes_ecuinfo += encoded_ecuinfo.length;
        }
    }

    struct encoded_data encoded_io_mnfst = encode_io_manifest(&ag_manifest->io);
    size_t num_bytes_io_mnfst = encoded_io_mnfst.length;

    size_t num_ecus_bytes = 8 + 1;

    size_t written = 0;
    unsigned char * buffer = (unsigned char *)malloc(num_ecus_bytes + num_bytes_ecuinfo + num_bytes_io_mnfst);
    written += cbor_serialize_uint(num_ecus, buffer + written, num_ecus_bytes);
    cbor_decref(&num_ecus);

    int i;
    for (i = 0; i < ag_manifest->num_ecus; ++i) {
        if (encoded_ecuinfos) {
            memcpy(buffer + written, encoded_ecuinfos[i].buff, encoded_ecuinfos[i].length);
            free(encoded_ecuinfos[i].buff);
            written += encoded_ecuinfos[i].length;
        }
    }

    memcpy(buffer + written, encoded_io_mnfst.buff, encoded_io_mnfst.length);
    written += encoded_io_mnfst.length;

    free(encoded_io_mnfst.buff);
    free(encoded_ecuinfos);
    encoded_agentmnfst.buff = buffer;
    encoded_agentmnfst.length = written;
    return encoded_agentmnfst;
}

size_t encode_heartbeat(const struct c2heartbeat * heartbeat, char ** buff, size_t * length) {
    struct encoded_data d_info = encode_deviceinfo(&heartbeat->device_info);
    struct encoded_data a_info = encode_agentinfo(&heartbeat->agent_info);

    cbor_item_t * has_ag_manifest = NULL;
    struct encoded_data ag_manifest;
    memset(&ag_manifest, 0, sizeof(ag_manifest));
    if (heartbeat->has_ag_manifest) {
        has_ag_manifest = cbor_build_uint8(1);
        ag_manifest = encode_agent_manifest(&heartbeat->ag_manifest);
    } else {
        has_ag_manifest = cbor_build_uint8(0);
    }
    char has_ag_manifest_buff[2];
    size_t has_ag_manifest_bytes = 1 + 1;
    size_t ag_manifest_bytes = cbor_serialize_uint(has_ag_manifest, has_ag_manifest_buff, 2);
    cbor_decref(&has_ag_manifest);

    char * serialized_hb = (char *)malloc((d_info.length + a_info.length + ag_manifest_bytes + ag_manifest.length) * sizeof(char));
    size_t offset = 0;
    memcpy(serialized_hb, d_info.buff, d_info.length);
    offset += d_info.length;
    memcpy(serialized_hb + offset, a_info.buff, a_info.length);
    offset += a_info.length;
    memcpy(serialized_hb + offset, has_ag_manifest_buff, ag_manifest_bytes);
    offset += ag_manifest_bytes;

    if(ag_manifest.length > 0) {
    	memcpy(serialized_hb + offset, ag_manifest.buff, ag_manifest.length);
    	offset += ag_manifest.length;
    }

    *buff = serialized_hb;
    *length = offset;

    free(d_info.buff);
    free(a_info.buff);
    free(ag_manifest.buff);
    return *length;
}

int decode_deviceinfo(const unsigned char * payload, size_t length, struct deviceinfo * dev_info, size_t * decoded_bytes) {
    if (!dev_info) {
        return -1;
    }
    size_t bytes_read = 0;
    struct cbor_load_result result;
    cbor_item_t * id_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (!id_item) return -1;
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(id_item)) {
        cbor_decref(&id_item);
        return -1;
    }
    bytes_read += result.read;
    unsigned char * id_str = cbor_string_handle(id_item);
    size_t id_length = cbor_string_length(id_item);
    dev_info->ident = (char *)malloc(id_length + 1);
    memset(dev_info->ident, 0, id_length + 1);
    strncpy(dev_info->ident, (char *)id_str, id_length);
    cbor_decref(&id_item);

    //decode system info
    cbor_item_t * sysinfo_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (!sysinfo_item) return -1;
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(sysinfo_item)) {
        cbor_decref(&sysinfo_item);
        return -1;
    }
    bytes_read += result.read;
    uint8_t has_sys_info =  cbor_get_uint8(sysinfo_item);
    cbor_decref(&sysinfo_item);

    if (has_sys_info) {
        cbor_item_t * arch_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(arch_item)) {
            cbor_decref(&arch_item);
            return -1;
        }
        bytes_read += result.read;
        const char * machine_arch = (const char *)cbor_string_handle(arch_item);
        size_t len = cbor_string_length(arch_item);
        strncpy(dev_info->system_info.machine_arch, machine_arch, len);
        cbor_decref(&arch_item);

        cbor_item_t * num_cores_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_cores_item)) {
            cbor_decref(&num_cores_item);
            return -1;
        }
        bytes_read += result.read;
        dev_info->system_info.v_cores = cbor_get_uint16(num_cores_item);
        cbor_decref(&num_cores_item);

        cbor_item_t * phys_mem_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(phys_mem_item)) {
            cbor_decref(&phys_mem_item);
            return -1;
        }
        bytes_read += result.read;
        dev_info->system_info.physical_mem = cbor_get_uint64(phys_mem_item);
        cbor_decref(&phys_mem_item);
    }

    //decode network_info
    cbor_item_t * net_info_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(net_info_item)) {
        cbor_decref(&net_info_item);
        return -1;
    }
    bytes_read += result.read;
    uint8_t has_net_info = cbor_get_uint8(net_info_item);
    cbor_decref(&net_info_item);
    if (has_net_info) {
        //decode device id
        cbor_item_t * dev_id_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(dev_id_item)) {
            cbor_decref(&dev_id_item);
            return -1;
        }
        bytes_read += result.read;
        const char * dev_id = (const char *)cbor_string_handle(dev_id_item);
        size_t dev_id_len = cbor_string_length(dev_id_item);
        dev_info->network_info.device_id = (char *)malloc(dev_id_len + 1);
        memset(dev_info->network_info.device_id, 0 , dev_id_len + 1);
        strncpy(dev_info->network_info.device_id, dev_id, dev_id_len);
        cbor_decref(&dev_id_item);

        //decode hostname
        cbor_item_t * host_name_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(host_name_item)) {
            cbor_decref(&host_name_item);
            return -1;
        }
        bytes_read += result.read;
        const char * host_name = (const char *)cbor_string_handle(host_name_item);
        size_t host_name_len = cbor_string_length(host_name_item);
        memset(dev_info->network_info.host_name, 0, sizeof(dev_info->network_info.host_name));
        strncpy(dev_info->network_info.host_name, host_name, host_name_len);
        cbor_decref(&host_name_item);

        //decode ip address
        cbor_item_t * ip_addr_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(ip_addr_item)) {
            cbor_decref(&ip_addr_item);
            return -1;
        }
        bytes_read += result.read;
        const char * ip_addr = (const char *)cbor_string_handle(ip_addr_item);
        size_t ip_addr_len = cbor_string_length(ip_addr_item);
        memset(dev_info->network_info.ip_address, 0, sizeof(dev_info->network_info.ip_address));
        strncpy(dev_info->network_info.ip_address, ip_addr, ip_addr_len);
        cbor_decref(&ip_addr_item);
    }
    *decoded_bytes = bytes_read;
    return 0;
}

int decode_agentinfo(const unsigned char * payload, size_t length, struct agentinfo  * agent_info, size_t * decoded_bytes) {
    if (!agent_info) {
        return -1;
    }
    size_t bytes_read = 0;

    struct cbor_load_result result;
    cbor_item_t * ag_info_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(ag_info_item)) {
        cbor_decref(&ag_info_item);
        return -1;
    }
    bytes_read += result.read;
    const char * agent_id = (const char *)cbor_string_handle(ag_info_item);
    size_t agent_id_len = cbor_string_length(ag_info_item);
    memset(agent_info->ident, 0, sizeof(agent_info->ident));
    strncpy(agent_info->ident, agent_id, agent_id_len);
    cbor_decref(&ag_info_item);

    cbor_item_t * agent_class_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(agent_class_item)) {
        cbor_decref(&agent_class_item);
        return -1;
    }
    bytes_read += result.read;
    const char * agent_class = (const char *)cbor_string_handle(agent_class_item);
    size_t agent_class_len = cbor_string_length(agent_class_item);
    agent_info->agent_class = (char *)malloc((agent_class_len + 1) * sizeof(char));
    memset(agent_info->agent_class, 0, agent_class_len + 1);
    strncpy(agent_info->agent_class, agent_class, agent_class_len);
    cbor_decref(&agent_class_item);

    cbor_item_t * uptime_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(uptime_item)) {
        cbor_decref(&uptime_item);
        return -1;
    }
    bytes_read += result.read;
    agent_info->uptime = cbor_get_uint64(uptime_item);
    cbor_decref(&uptime_item);
    *decoded_bytes = bytes_read;
    return 0;
}

int decode_agentmanifest(const unsigned char * payload, size_t length, struct agent_manifest * ag_manifest, size_t * bytes_decoded) {
    if (!ag_manifest) {
        return -1;
    }

    size_t bytes_read = 0;

    struct cbor_load_result result;
    cbor_item_t * ag_mnfst_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(ag_mnfst_item)) {
        cbor_decref(&ag_mnfst_item);
        return -1;
    }
    bytes_read += result.read;
    uint8_t has_ag_mnfst = cbor_get_uint8(ag_mnfst_item);
    cbor_decref(&ag_mnfst_item);

    if (has_ag_mnfst) {
        cbor_item_t * num_ecus_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_ecus_item)) {
            cbor_decref(&num_ecus_item);
            return -1;
        }
        bytes_read += result.read;
        uint64_t num_ecus = cbor_get_uint64(num_ecus_item);
        cbor_decref(&num_ecus_item);
        ag_manifest->num_ecus = num_ecus;

        if (num_ecus) {
            ag_manifest->ecus = (ecuinfo *)malloc(num_ecus * sizeof(ecuinfo));
            int i;
            for (i = 0; i < num_ecus; ++i) {
                cbor_item_t * uuid_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(uuid_item)) {
                    cbor_decref(&uuid_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * uuid = (const char *)cbor_string_handle(uuid_item);
                size_t uuid_len = cbor_string_length(uuid_item);
                memcpy(ag_manifest->ecus[i].uuid, uuid, uuid_len);
                ag_manifest->ecus[i].uuid[uuid_len] = '\0';
                cbor_decref(&uuid_item);

                cbor_item_t * name_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(name_item)) {
                    cbor_decref(&name_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * name = (const char *)cbor_string_handle(name_item);
                size_t name_len = cbor_string_length(name_item);
                ag_manifest->ecus[i].name = (char *)malloc(name_len + 1);
                memset(ag_manifest->ecus[i].name, 0, name_len + 1);
                strncpy((char *)ag_manifest->ecus[i].name, name, name_len);
                cbor_decref(&name_item);

                cbor_item_t * input_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(input_item)) {
                    cbor_decref(&input_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * input = (const char *)cbor_string_handle(input_item);
                size_t input_len = cbor_string_length(input_item);
                ag_manifest->ecus[i].input = (char *)malloc(input_len + 1);
                memset(ag_manifest->ecus[i].input, 0, input_len + 1);
                strncpy((char *)ag_manifest->ecus[i].input, input, input_len);
                cbor_decref(&input_item);

                cbor_item_t * ip_args_map = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_map_is_definite(ip_args_map)) {
                    cbor_decref(&ip_args_map);
                    return -1;
                }
                bytes_read += result.read;
                size_t num_ip_args = cbor_map_size(ip_args_map);
                struct cbor_pair * props = cbor_map_handle(ip_args_map);
                int k;
                for (k = 0; k < num_ip_args; ++k) {
                    properties_t * prop = (properties_t *)malloc(sizeof(properties_t));
                    size_t ks = cbor_string_length(props[k].key);
                    size_t vs = cbor_string_length(props[k].value);
                    prop->key = (char *)malloc(ks + 1);
                    memset(prop->key, 0, ks + 1);
                    strncpy(prop->key, (const char *)cbor_string_handle(props[k].key), ks);
                    prop->value = (char *)malloc(vs + 1);
                    memset(prop->value, 0, vs + 1);
                    strncpy(prop->value, (const char *)cbor_string_handle(props[k].value), vs);
                    HASH_ADD_KEYPTR(hh, ag_manifest->ecus[i].ip_args, prop->key, strlen(prop->key), prop);
                }
                cbor_decref(&ip_args_map);

                cbor_item_t * output_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(output_item)) {
                    cbor_decref(&output_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * output = (const char *)cbor_string_handle(output_item);
                size_t output_len = cbor_string_length(output_item);
                ag_manifest->ecus[i].output = (char *)malloc(output_len + 1);
                memset(ag_manifest->ecus[i].output, 0, output_len + 1);
                strncpy((char *)ag_manifest->ecus[i].output, output, output_len);
                cbor_decref(&output_item);

                cbor_item_t * op_args_map = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_map_is_definite(op_args_map)) {
                    cbor_decref(&op_args_map);
                    return -1;
                }
                bytes_read += result.read;
                size_t num_op_args = cbor_map_size(op_args_map);
                struct cbor_pair * op_props = cbor_map_handle(op_args_map);
                for (k = 0; k < num_op_args; ++k) {
                    properties_t * prop = (properties_t *)malloc(sizeof(properties_t));
                    size_t ks = cbor_string_length(op_props[k].key);
                    size_t vs = cbor_string_length(op_props[k].value);
                    prop->key = (char *)malloc(ks + 1);
                    memset(prop->key, 0, ks + 1);
                    const char * key = (const char *)cbor_string_handle(op_props[k].key);
                    strncpy(prop->key, key, ks);
                    prop->value = (char *)malloc(vs + 1);
                    memset(prop->value, 0, vs + 1);
                    const char * value = (const char *)cbor_string_handle(op_props[k].value);
                    strncpy(prop->value, value, vs);
                    HASH_ADD_KEYPTR(hh, ag_manifest->ecus[i].op_args, prop->key, strlen(prop->key), prop);
                }
                cbor_decref(&op_args_map);
            }
        }

        cbor_item_t * num_ips_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_ips_item)) {
            cbor_decref(&num_ips_item);
            return -1;
        }
        bytes_read += result.read;
        uint8_t num_ips = cbor_get_uint8(num_ips_item);
        cbor_decref(&num_ips_item);

        if (num_ips) {
            ag_manifest->io.num_ips = num_ips;
            ag_manifest->io.input_params = (ioparams *)malloc(num_ips * sizeof(ioparams));
            int i;
            for (i = 0; i < num_ips; ++i) {
                cbor_item_t * ip_name_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(ip_name_item)) {
                    cbor_decref(&ip_name_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * input_name = (const char *)cbor_string_handle(ip_name_item);
                size_t input_len = cbor_string_length(ip_name_item);
                ag_manifest->io.input_params[i].name = (char *)malloc(input_len + 1);
                memset(ag_manifest->io.input_params[i].name, 0, input_len + 1);
                strncpy(ag_manifest->io.input_params[i].name, input_name, input_len);
                cbor_decref(&ip_name_item);

                cbor_item_t * num_params_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_params_item)) {
                    cbor_decref(&num_params_item);
                    return -1;
                }
                bytes_read += result.read;
                uint16_t num_params = cbor_get_uint16(num_params_item);
                cbor_decref(&num_params_item);
                ag_manifest->io.input_params[i].num_params = num_params;

                ag_manifest->io.input_params[i].params = (char **)malloc(num_params * sizeof(char *));
                int j;
                for (j = 0; j < num_params; ++j) {
                    cbor_item_t * param_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(param_item)) {
                        cbor_decref(&param_item);
                        return -1;
                    }
                    bytes_read += result.read;
                    size_t pl = cbor_string_length(param_item);
                    ag_manifest->io.input_params[i].params[j] = (char *)malloc(pl + 1);
                    const char * param_str = (const char *)cbor_string_handle(param_item);
                    memset(ag_manifest->io.input_params[i].params[j], 0, pl + 1);
                    strncpy(ag_manifest->io.input_params[i].params[j], param_str, pl);
                    cbor_decref(&param_item);
                }
            }
        }

        cbor_item_t * num_ops_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_ops_item)) {
            cbor_decref(&num_ops_item);
            return -1;
        }
        bytes_read += result.read;
        uint8_t num_ops = cbor_get_uint8(num_ops_item);
        cbor_decref(&num_ops_item);

        if (num_ops) {
            ag_manifest->io.num_ops = num_ops;
            ag_manifest->io.output_params = (ioparams *)malloc(num_ops * sizeof(ioparams));
            int i;
            for (i = 0; i < num_ops; ++i) {
                cbor_item_t * op_name_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(op_name_item)) {
                    cbor_decref(&op_name_item);
                    return -1;
                }
                bytes_read += result.read;
                const char * output_name = (const char *)cbor_string_handle(op_name_item);
                size_t output_len = cbor_string_length(op_name_item);
                ag_manifest->io.output_params[i].name = (char *)malloc(output_len + 1);
                memset(ag_manifest->io.output_params[i].name, 0, output_len + 1);
                strncpy(ag_manifest->io.output_params[i].name, output_name, output_len);
                cbor_decref(&op_name_item);

                cbor_item_t * num_params_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(num_params_item)) {
                    cbor_decref(&num_params_item);
                    return -1;
                }
                bytes_read += result.read;
                uint16_t num_params = cbor_get_uint16(num_params_item);
                cbor_decref(&num_params_item);
                ag_manifest->io.output_params[i].num_params = num_params;

                ag_manifest->io.output_params[i].params = (char **)malloc(num_params * sizeof(char *));
                int j;
                for (j = 0; j < num_params; ++j) {
                    cbor_item_t * param_item = cbor_load(payload + bytes_read, length - bytes_read, &result);
                    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(param_item)) {
                        cbor_decref(&param_item);
                        return -1;
                    }
                    bytes_read += result.read;
                    size_t pl = cbor_string_length(param_item);
                    ag_manifest->io.output_params[i].params[j] = (char *)malloc(pl + 1);
                    memset(ag_manifest->io.output_params[i].params[j], 0, (pl + 1));
                    const char * param_str = (const char *)cbor_string_handle(param_item);
                    strncpy(ag_manifest->io.output_params[i].params[j], param_str, pl);
                    cbor_decref(&param_item);
                }
            }
        }
    }
    *bytes_decoded = bytes_read;
    return 0;
}

int decode_heartbeat(const unsigned char * payload, size_t length, struct c2heartbeat * heartbeat, size_t * bytes_decoded) {
    if (!heartbeat) {
        return -1;
    }
    size_t bytes_read = 0;

    struct cbor_load_result result;
    cbor_item_t * cbor_item_type = cbor_load(payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(cbor_item_type)) {
        cbor_decref(&cbor_item_type);
        return -1;
    }
    bytes_read += result.read;
    uint8_t is_heartbeat = cbor_get_uint8(cbor_item_type);
    cbor_decref(&cbor_item_type);

    if (!is_heartbeat) {
        return -1;
    }
    size_t dev_info_bytes = 0;
    if (decode_deviceinfo(payload + bytes_read, length - bytes_read, &heartbeat->device_info, &dev_info_bytes) < 0) {
        free_c2heartbeat(heartbeat);
        return -1;
    }
    bytes_read += dev_info_bytes;

    size_t agent_info_bytes = 0;
    if (decode_agentinfo(payload + bytes_read, length - bytes_read, &heartbeat->agent_info, &agent_info_bytes) < 0) {
        free_c2heartbeat(heartbeat);
        return -1;
    }
    bytes_read += agent_info_bytes;

    size_t agent_manifest_bytes = 0;
    if (decode_agentmanifest(payload + bytes_read, length - bytes_read, &heartbeat->ag_manifest, &agent_manifest_bytes) < 0) {
        free_c2heartbeat(heartbeat);
        return -1;
    }
    if (agent_manifest_bytes > 0) {
        heartbeat->has_ag_manifest = 1;
    }
    bytes_read += agent_manifest_bytes;

    *bytes_decoded = bytes_read;
    return 0;
}

uint16_t endian_check_uint16(uint16_t value, int is_little_endian) {
    unsigned char buf[2];
    memcpy(buf, &value, 2);
    if (is_little_endian) {
        return (buf[0] << 8) | buf[1];
    }
    return buf[0] | (buf[1] << 8);
}

uint32_t endian_check_uint32(uint32_t value, int is_little_endian) {
    unsigned char buf[4];
    memcpy(buf, &value, 4);
    if (is_little_endian) {
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    }
    return buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
}

uint16_t endian_check_uint64(uint64_t value, int is_little_endian) {
    unsigned char buf[8];
    memcpy(buf, &value, 8);
    if (is_little_endian) {
        return ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
                | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
    }
    return ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
           | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
}

c2operation get_operation(uint8_t type) {
    switch (type) {
        case 0:
            return ACKNOWLEDGE;
        case 1:
            return HEARTBEAT;
        case 2:
            return CLEAR;
        case 3:
            return DESCRIBE;
        case 4:
            return RESTART;
        case 5:
            return START;
        case 6:
            return UPDATE;
        case 7:
            return STOP;
    }
    return ACKNOWLEDGE;
}

c2_server_response_t * decode_c2_server_response(const struct coap_message * msg, int is_little_endian) {
    if (!msg) {
        return NULL;
    }
    unsigned char * c2payload = msg->data;
    size_t length = msg->length;

    size_t bytes_read = 0;
    struct cbor_load_result result;
    struct cbor_item_t * ver_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(ver_item)) {
        return NULL;
    }
    bytes_read += result.read;
    const uint16_t version = endian_check_uint16(cbor_get_uint16(ver_item), is_little_endian);
    cbor_decref(&ver_item);

    struct cbor_item_t * size_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(size_item)) {
        return NULL;
    }
    bytes_read += result.read;
    const uint16_t size = endian_check_uint16(cbor_get_uint16(size_item), is_little_endian);
    cbor_decref(&size_item);

    uint8_t operation_type;
    uint16_t argsize = 0;
    char * operand = NULL;
    char * id = NULL;

    c2_server_response_t * response_list = NULL;
    int i;
    for (i = 0; i < size; ++i) {
        struct cbor_item_t * optype_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(optype_item)) {
            cbor_decref(&optype_item);
            return NULL;
        }
        bytes_read += result.read;
        operation_type = cbor_get_uint8(optype_item);
        cbor_decref(&optype_item);

        struct cbor_item_t * id_type = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(id_type)) {
            cbor_decref(&id_type);
            return NULL;
        }
        bytes_read += result.read;
        size_t id_len = cbor_string_length(id_type);
        id = (char *)malloc(id_len + 1);
        memset(id, 0, id_len + 1);
        const char * id_handle = (const char *)cbor_string_handle(id_type);
        strncpy(id, id_handle, id_len);
        cbor_decref(&id_type);

        struct cbor_item_t * operand_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(operand_item)) {
            cbor_decref(&operand_item);
            return NULL;
        }
        bytes_read += result.read;
        size_t operand_len = cbor_string_length(operand_item);
        operand = (char *)malloc(operand_len + 1);
        memset(operand, 0, operand_len + 1);
        const char * operand_handle = (const char *)cbor_string_handle(operand_item);
        strncpy(operand, operand_handle, operand_len);
        cbor_decref(&operand_item);

        struct cbor_item_t * argsize_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
        if (result.error.code != CBOR_ERR_NONE || !cbor_isa_uint(argsize_item)) {
            cbor_decref(&argsize_item);
            return NULL;
        }
        bytes_read += result.read;
        argsize = endian_check_uint16(cbor_get_uint16(argsize_item), is_little_endian);
        cbor_decref(&argsize_item);

        c2_server_response_t * response = (c2_server_response_t *)malloc(sizeof(c2_server_response_t));
        memset(response, 0, sizeof(c2_server_response_t));
        response->ident = (char *)malloc(strlen(id) + 1);
        strcpy(response->ident, id);
        free(id);
        response->operand = operand;
        response->operation = get_operation(operation_type);

        int j;
        for (j = 0; j < argsize; ++j) {
            struct cbor_item_t * key_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
            if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(key_item)) {
                free_c2_server_responses(response);
                free_c2_server_responses(response_list);
                cbor_decref(&key_item);
                return NULL;
            }
            bytes_read += result.read;
            const char * key_handle = (const char *)cbor_string_handle(key_item);
            size_t key_len = cbor_string_length(key_item);
            char * key = (char *)malloc(key_len + 1);
            memset(key, 0, key_len + 1);
            strncpy(key, key_handle, key_len);
            cbor_decref(&key_item);

            struct cbor_item_t * value_item = cbor_load(c2payload + bytes_read, length - bytes_read, &result);
            if (result.error.code != CBOR_ERR_NONE || !cbor_isa_string(value_item)) {
                free_c2_server_responses(response);
                free_c2_server_responses(response_list);
                free(key);
                cbor_decref(&value_item);
                return NULL;
            }
            bytes_read += result.read;
            const char * value_handle = (const char *)cbor_string_handle(value_item);
            size_t value_len = cbor_string_length(value_item);
            char * value = (char *)malloc(value_len + 1);
            memset(value, 0, value_len + 1);
            strncpy(value, value_handle, value_len);
            cbor_decref(&value_item);

            properties_t * args = NULL;
            HASH_FIND_STR(response->args, key, args);
            if (args) {
                char * tmp = args->value;
                free(tmp);
                args->value = value;
            } else {
                args = (properties_t *)malloc(sizeof(properties_t));
                args->key = key;
                args->value = value;
                HASH_ADD_KEYPTR(hh, response->args, key, strlen(key), args);
            }
        }
        LL_APPEND(response_list, response);
    }
    return response_list;
}

void encode_c2_response(const c2_response_t * resp, char ** buff, size_t * length) {
    cbor_item_t * id = cbor_build_string(resp->ident);
    size_t id_len = cbor_string_length(id);

    cbor_item_t * operation = cbor_build_uint8((uint8_t)resp->operation);
    size_t operation_bytes = 1 + 1;

    unsigned char * buffer = (unsigned char *)malloc(id_len + 8 + 1 + operation_bytes);
    size_t written = 0;
    written += cbor_serialize_string(id, buffer, id_len + 8 + 1);
    written += cbor_serialize_uint(operation, buffer + written, operation_bytes);
    cbor_decref(&id);
    cbor_decref(&operation);

    *buff = (char *)buffer;
    *length = written;
}

void reallocate_buffer(unsigned char ** buffer, size_t sub_length, size_t copy_bytes, size_t * total_len) {
    if (sub_length - (*total_len * 2) > 0) {
        *total_len = (*total_len * 2) + sub_length;
    }
    else {
        *total_len *= 2;
    }
    unsigned char * new_buffer = (unsigned char *)malloc(*total_len);
    //copy the contents of buffer into new buffer and deallocate buffer
    memcpy(new_buffer, *buffer, copy_bytes);
    unsigned char * tmp = *buffer;
    free(tmp);
    *buffer = new_buffer;
}

void encode_c2_server_response(c2_server_response_t * response, char ** buff, size_t * length) {
    size_t total_len = 1024;

    //encode version
    uint16_t version = 1;
    struct cbor_item_t * ver_item = cbor_build_uint16(htons((uint16_t)version));
    size_t ver_bytes = 2 + 1;

    //encode size of c2 response list
    size_t s = 0;
    c2_server_response_t * head = response;
    c2_server_response_t * el = NULL;
    LL_COUNT(head, el, s);
    struct cbor_item_t * list_size_item = cbor_build_uint16(htons((uint16_t) (s)));
    size_t size_bytes = 2 + 1;

    //create a buffer of total_len to encode data
    unsigned char * buffer = (unsigned char *)malloc(total_len);

    size_t written = 0; //number of bytes written into the buffer

    written += cbor_serialize_uint(ver_item, buffer + written, ver_bytes);
    written += cbor_serialize_uint(list_size_item, buffer + written, size_bytes);
    cbor_decref(&ver_item);
    cbor_decref(&list_size_item);

    while (head) {
        size_t sub_length = 0;
        //encode operation type
        struct cbor_item_t * op_item = cbor_build_uint8(head->operation);
        size_t op_bytes = 1 + 1;

        struct cbor_item_t * id_item = cbor_build_string(head->ident);
        size_t id_bytes = cbor_string_length(id_item) + 8 + 1;

        struct cbor_item_t *operand_item = cbor_build_string(head->operand);
        size_t operand_bytes = cbor_string_length(operand_item) + 8 + 1;

        size_t args_size = HASH_COUNT(head->args);
        struct cbor_item_t *args_sz_item = cbor_build_uint16(htons((uint16_t) (args_size)));
        size_t args_sz_bytes = 2 + 1;

        sub_length += (op_bytes + id_bytes + operand_bytes + args_sz_bytes);

        if (sub_length > (total_len - written)) {
            //we need reallocation of buffer into a larger buffer size
            reallocate_buffer(&buffer, sub_length, written, &total_len);
        }

        written += cbor_serialize_uint(op_item, buffer + written, op_bytes);
        written += cbor_serialize_string(id_item, buffer + written, id_bytes);
        written += cbor_serialize_string(operand_item, buffer + written, operand_bytes);
        written += cbor_serialize_uint(args_sz_item, buffer + written, args_sz_bytes);

        cbor_decref(&op_item);
        cbor_decref(&id_item);
        cbor_decref(&operand_item);
        cbor_decref(&args_sz_item);

        properties_t * op_args;
        for (op_args = head->args; op_args; op_args = op_args->hh.next) {
            struct cbor_item_t * key_item = cbor_build_string(op_args->key);
            struct cbor_item_t * value_item = cbor_build_string(op_args->value);
            size_t key_bytes = cbor_string_length(key_item) + 8 + 1;
            size_t value_bytes = cbor_string_length(value_item) + 8 + 1;
            size_t kv_bytes = key_bytes + value_bytes;
            if (kv_bytes > (total_len - written)) {
                reallocate_buffer(&buffer, kv_bytes, written, &total_len);
            }
            written += cbor_serialize_string(key_item, buffer + written, key_bytes);
            written += cbor_serialize_string(value_item, buffer + written, value_bytes);
            cbor_decref(&key_item);
            cbor_decref(&value_item);
        }
        head = head->next;
    }

    *buff = (char *) buffer;
    *length = written;
}

#ifdef __cplusplus
}
#endif
