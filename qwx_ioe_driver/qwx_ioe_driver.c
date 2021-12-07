/* Copyright 2021, Esteban Volentini - Facet UNT, FiUBA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** @file qwx_ioe_driver.c
 **
 ** @brief Driver para el kernel unix para la placa QWXIOE
 **
 ** Este controlador agrega sooutpute a nivel de dispositivo linux de la placa
 ** para expancion de entrada/salida .
 ** 
 **| REV | YYYY.MM.DD | Autor           | Descripción de los cambios                              |
 **|-----|------------|-----------------|---------------------------------------------------------|
 **|   1 | 2016.06.25 | evolentini      | Version inicial del archivo                             |
 ** 
 ** @defgroup plataforma 
 ** @brief Controladores de dispositivos
 ** @{ 
 */

/* === Inclusiones de cabeceras ================================================================ */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>

/* === Definiciones y Macros =================================================================== */

#define OUTPUTS_COUNT   3

#define READERS_COUNT   2

/* === Declaraciones de tipos de datos internos ================================================ */

//! Estructura con la informacion del dispositivo correspondiente a la placa de expansion
struct expansion_dev {
    struct i2c_client *client;
    struct miscdevice outputs[OUTPUTS_COUNT];
    struct miscdevice readers[READERS_COUNT];
    char name[I2C_NAME_SIZE];
    int device;
};

/* === Declaraciones de funciones internas ===================================================== */

static ssize_t output_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos);

static ssize_t output_write(struct file *file, const char __user *buffer, size_t len, loff_t *f_pos);

static ssize_t reader_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos);

static int probe(struct i2c_client *client, const struct i2c_device_id *id);

static int remove(struct i2c_client * client);

/* === Definiciones de variables internas ====================================================== */

/* === Definiciones de variables externas ====================================================== */

MODULE_AUTHOR("Esteban Volentini <evolentini@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Controlador de disposativo para la placa de expansión");

//! Arreglo con la lista de identificadores de dispositivos compatibles con el controlador
static const struct of_device_id compatibles_devices_id[] = {
    { .compatible = "equiser,qwxioe", },
    { /* sentinel */ }
}; 

//! Declaracion para el kernel de la lista de dispositivos compatibles con el controlador
MODULE_DEVICE_TABLE(of, compatibles_devices_id);

//! Estrcutura con la implementación del controlador de dispositivo para el cliente I2C
static struct i2c_driver device_driver = {
    .probe = probe,
    .remove = remove,
    .driver = {
        .name = "qwx_ioe_driver",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(compatibles_devices_id),
    },
};

//! Estructura con la implementacion las operaciones de archivos en salidas digitales
static const struct file_operations outputs_fops = {
    .owner = THIS_MODULE,
    .read = output_read,
    .write = output_write,
};

//! Estructura con la implementacion las operaciones de archivos en lectoras de rfid
static const struct file_operations readers_fops = {
    .owner = THIS_MODULE,
    .read = reader_read,
};

/* === Definiciones de funciones internas ====================================================== */

static ssize_t output_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos)  {  
    unsigned short int output = file->f_path.dentry->d_name.name[1] - '0';
    struct expansion_dev * device = container_of(file->private_data, struct expansion_dev, outputs[output]);
    char data[3] = "0\n";
    char address, response;

    if (*f_pos == 0) {
        address = 0x70 + output;
        i2c_master_send(device->client, &address, sizeof(address));

        i2c_master_recv(device->client, &response, sizeof(response));
        data[0] += response;

        count = sizeof(data);
        if (copy_to_user(buffer, data, count)) {
            return -EFAULT;
        }

        *f_pos += count;
        return count;
    }
    return 0;
}

static ssize_t output_write(struct file *file, const char __user *buffer, size_t len, loff_t *f_pos)  {    
    unsigned short int output = file->f_path.dentry->d_name.name[1] - '0';
    struct expansion_dev * device = container_of(file->private_data, struct expansion_dev, outputs[output]);
    char data[2];
    char response;
    
    if (len == 0) {
        return 0;
    }
    if (copy_from_user(&response, buffer, 1)) {
        return -EFAULT;
    }

    if (response == '1') {
        data[0] = 0x71;
    } else {
        data[0] = 0x70;
    }

    data[1] = output;
    i2c_master_send(device->client, data, 2);

    return len;
}

static ssize_t reader_read(struct file *file, char __user *buffer, size_t count, loff_t *f_pos)  {  
    unsigned short int reader = file->f_path.dentry->d_name.name[1] - '0';
    struct expansion_dev * device = container_of(file->private_data, struct expansion_dev, readers[reader]);
    char address, response[8];
    uint card_number;
    char data[12];

    if (*f_pos == 0) {
        address = 0x10 + 0x08 * reader;
        i2c_master_send(device->client, &address, 1);

        memset(data, 0, sizeof(response));
        i2c_master_recv(device->client, response, sizeof(response));
        card_number = ((uint)response[2] << 16) + ((uint)response[1] << 8) + response[0];
        snprintf(data, sizeof(data), "%d\n", card_number);

        count = sizeof(data);
        if (copy_to_user(buffer, data, count)) {
            return -EFAULT;
        }

        *f_pos += count;
        return count;
    }
    return 0;
}

int add_output(struct expansion_dev *device, unsigned short int output_number) {
    struct miscdevice *output = &device->outputs[output_number];
    char *name;

    name = devm_kzalloc(&device->client->dev, I2C_NAME_SIZE, GFP_KERNEL);
    snprintf(name, I2C_NAME_SIZE, "%s/s%d", device->name, output_number);
    output->name = name;
    output->minor = MISC_DYNAMIC_MINOR;
    output->fops = &outputs_fops;

    return misc_register(output);
}

int add_reader(struct expansion_dev *device, unsigned short int reader_number) {
    struct miscdevice *reader = &device->readers[reader_number];
    char *name;

    name = devm_kzalloc(&device->client->dev, I2C_NAME_SIZE, GFP_KERNEL);
    snprintf(name, I2C_NAME_SIZE, "%s/w%d", device->name, reader_number);
    reader->name = name;
    reader->minor = MISC_DYNAMIC_MINOR;
    reader->fops = &readers_fops;

    return misc_register(reader);
}

static int probe(struct i2c_client *client, const struct i2c_device_id *id)  {
    struct expansion_dev *device;
    int error, output, reader, index;

    if (client->addr == 0x50) {
        device = devm_kzalloc(&client->dev, sizeof(struct expansion_dev), GFP_KERNEL);
        snprintf(device->name, I2C_NAME_SIZE, "/exp0");
    } else {
        pr_err("No se reconoce la direccion del dispositivo");
        return error;
    }
    device->client = client;
    i2c_set_clientdata(client, device);

    for(output = 0; output < OUTPUTS_COUNT; output++) {
        error = add_output(device, output);
        if (error != 0) {
            pr_err("No se pudo registrar el dispositivo %s/s%d", device->name, output);
            for(index = 0; index < output; index++) {
                misc_deregister(&device->outputs[index]);
            }
            return error;
        }
    }

    for(reader = 0; reader < READERS_COUNT; reader++) {
        error = add_reader(device, reader);
        if (error != 0) {
            pr_err("No se pudo registrar el dispositivo %s/w%d", device->name, output);
            for(index = 0; index < reader; index++) {
                misc_deregister(&device->readers[index]);
            }
            for(output = 0; output < OUTPUTS_COUNT; output++) {
                misc_deregister(&device->outputs[output]);
            }
            return error;
        }
    }

    return 0;
}

static int remove(struct i2c_client * client)  {
    struct expansion_dev *device = i2c_get_clientdata(client);
    int output, reader;

    for(output = 0; output < OUTPUTS_COUNT; output++) {
        misc_deregister(&device->outputs[output]);
    }
    for(reader = 0; reader < READERS_COUNT; reader++) {
        misc_deregister(&device->readers[reader]);
    }

    return 0;
}

/* === Definiciones de funciones externas ====================================================== */

module_i2c_driver(device_driver);

/* === Ciere de documentacion ================================================================== */
/** @} Final de la definición del modulo para doxygen */
