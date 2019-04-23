<img src="https://ucbrise.github.io/opaque/opaque.svg" width="315" alt="Opaque">

**Secure Apache Spark SQL**

[![Build Status](https://travis-ci.org/ucbrise/opaque.svg?branch=master)](https://travis-ci.org/ucbrise/opaque)

Opaque is a package for Apache Spark SQL that enables encryption for DataFrames using Intel SGX trusted hardware. The aim is to enable analytics on sensitive data in an untrusted cloud. Once the contents of a DataFrame are encrypted, subsequent operations will run within SGX enclaves.

This project is based on our NSDI 2017 paper [1]. The oblivious execution mode is not included in this release.

This is an alpha preview of Opaque, which means the software is still in development (not production-ready!). It currently has the following limitations:

- Unlike the Spark cluster, the master must be run within a trusted environment (e.g., on the client).

- Not all Spark SQL operations are supported. UDFs are currently not supported.

- Computation integrity verification (section 4.2 of the NSDI paper) is not included.

[1] Wenting Zheng, Ankur Dave, Jethro Beekman, Raluca Ada Popa, Joseph Gonzalez, and Ion Stoica.
[Opaque: An Oblivious and Encrypted Distributed Analytics Platform](https://people.eecs.berkeley.edu/~wzheng/opaque.pdf). NSDI 2017, March 2017.

## Installation

After downloading the Opaque codebase, build and test it as follows. (Alternatively, we offer a [Docker image](docker/) that contains a prebuilt version of Opaque.)

1. Install dependencies and the [Intel SGX SDK](https://01.org/intel-software-guard-extensions/downloads):

    ```sh
    # For Ubuntu 16.04 or 18.04:
    sudo apt install wget build-essential openjdk-8-jdk python cmake libssl-dev

    # For Ubuntu 16.04:
    wget -O sgx_installer.bin https://download.01.org/intel-sgx/linux-2.3.1/ubuntu16.04/sgx_linux_x64_sdk_2.3.101.46683.bin
    # For Ubuntu 18.04:
    wget -O sgx_installer.bin https://download.01.org/intel-sgx/linux-2.3.1/ubuntu18.04/sgx_linux_x64_sdk_2.3.101.46683.bin

    # Installer will prompt for install path, which can be user-local
    chmod +x ./sgx_installer.bin
    ./sgx_installer.bin

    source sgxsdk/environment
    ```

2. On the master, generate a keypair using OpenSSL for remote attestation. The public key will be automatically hardcoded into the enclave code.
   Note that only the NIST p-256 curve is supported.

    ```sh
    cd ${OPAQUE_HOME}
    openssl ecparam -name prime256v1 -genkey -noout -out private_key.pem
    ```

3. Set the following environment variables:

    ```sh
    export SPARKSGX_DATA_DIR=${OPAQUE_HOME}/data
    export PRIVATE_KEY_PATH=${OPAQUE_HOME}/private_key.pem
    ```

    If running with real SGX hardware, also set `export SGX_MODE=HW`.

4. Run the Opaque tests:

    ```sh
    cd ${OPAQUE_HOME}
    build/sbt test
    ```

## Usage

Next, run Apache Spark SQL queries with Opaque as follows, assuming [Spark 2.4.0](https://github.com/apache/spark/releases/tag/v2.4.0) is already installed:

1. Package Opaque into a JAR:

    ```sh
    cd ${OPAQUE_HOME}
    build/sbt package
    ```

2. Launch the Spark shell with Opaque:

    ```sh
    ${SPARK_HOME}/bin/spark-shell --jars ${OPAQUE_HOME}/target/scala-2.11/opaque_2.11-0.1.jar
    ```
    
    Alternatively, to run Opaque queries locally for development rather than on a cluster:
    
    ```sh
    cd ${OPAQUE_HOME}
    JVM_OPTS="-Xmx4G" build/sbt console
    ```

3. Inside the Spark shell, import Opaque's DataFrame methods and install Opaque's query planner rules:

    ```scala
    import edu.berkeley.cs.rise.opaque.implicits._

    edu.berkeley.cs.rise.opaque.Utils.initSQLContext(spark.sqlContext)
    ```

4. Create an encrypted DataFrame:

    ```scala
    val data = Seq(("foo", 4), ("bar", 1), ("baz", 5))
    val df = spark.createDataFrame(data).toDF("word", "count")
    val dfEncrypted = df.encrypted
    ```

5. Query the DataFrames and explain the query plan to see the secure operators:


    ```scala
    dfEncrypted.filter($"count" > lit(3)).explain(true)
    // [...]
    // == Optimized Logical Plan ==
    // EncryptedFilter (count#6 > 3)
    // +- EncryptedLocalRelation [word#5, count#6]
    // [...]

    dfEncrypted.filter($"count" > lit(3)).show
    // +----+-----+
    // |word|count|
    // +----+-----+
    // | foo|    4|
    // | baz|    5|
    // +----+-----+
    ```

6. Save and load an encrypted DataFrame:

    ```scala
    dfEncrypted.write.format("edu.berkeley.cs.rise.opaque.EncryptedSource").save("dfEncrypted")
    // The file dfEncrypted/part-00000 now contains encrypted data

    import org.apache.spark.sql.types._
    val df2 = (spark.read.format("edu.berkeley.cs.rise.opaque.EncryptedSource")
      .schema(StructType(Seq(StructField("word", StringType), StructField("count", IntegerType))))
      .load("dfEncrypted"))
    df2.show
    // +----+-----+
    // |word|count|
    // +----+-----+
    // | foo|    4|
    // | bar|    1|
    // | baz|    5|
    // +----+-----+
    ```
    
## Launch Token and Remote Attestation

For development, Opaque launches enclaves in debug mode. To launch enclaves in release mode, use a [Launch Enclave](https://github.com/intel/linux-sgx/blob/master/psw/ae/ref_le/ref_le.md) or contact Intel to obtain a launch token, then pass it to `sgx_create_enclave` in `src/enclave/App/App.cpp`. Additionally, change `-DEDEBUG` to `-UEDEBUG` in `src/enclave/CMakeLists.txt`.

Remote attestation ensures that the workers' SGX enclaves are genuine. To use remote attestation, do the following:

1. [Generate a self-signed certificate](https://software.intel.com/en-us/articles/how-to-create-self-signed-certificates-for-use-with-intel-sgx-remote-attestation-using):

    ```sh
    cat <<EOF > client.cnf
    [ ssl_client ]
    keyUsage = digitalSignature, keyEncipherment, keyCertSign
    subjectKeyIdentifier=hash
    authorityKeyIdentifier=keyid,issuer
    extendedKeyUsage = clientAuth, serverAuth
    EOF

    openssl genrsa -out client.key 2048
    openssl req -key client.key -new -out client.req
    openssl x509 -req -days 365 -in client.req -signkey client.key -out client.crt -extfile client.cnf -extensions ssl_client
    openssl pkcs12 -export -out client.pfx -inkey client.key -in client.crt
    
    # Should print "client.crt: OK"
    openssl verify -x509_strict -purpose sslclient -CAfile client.crt client.crt
    ```
    
2. Upload the certificate to the [Intel SGX Development Services Access Request form](https://software.intel.com/en-us/form/sgx-onboarding) and wait for a response from Intel, which should include a certificate.

3. Set the following environment variables:

    ```sh
    # Require attestation to complete successfully before sending secrets to the worker enclaves.
    export OPAQUE_REQUIRE_ATTESTATION=1

    export IAS_CLIENT_CERT_FILE=.../client.crt  # from openssl x509 above
    export IAS_CLIENT_KEY_FILE=.../client.key   # from openssl genrsa above
    export IAS_REPORT_SIGNING_FILE=...   # Intel-provided certificate from step 2
    ```

4. Change the value of `Utils.sharedKey` (`src/main/scala/edu/berkeley/cs/rise/opaque/Utils.scala`), the shared data encryption key. Opaque will ensure that each enclave passes remote attestation before sending it this key.

## Contact

If you want to know more about our project or have questions, please contact Wenting (wzheng@eecs.berkeley.edu) and/or Ankur (ankurdave@gmail.com).
