/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package edu.berkeley.cs.rise.opaque.benchmark

import edu.berkeley.cs.rise.opaque.Utils
import org.apache.spark.sql.DataFrame
import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.types._
import org.apache.spark.internal.Logging

object TriangleCounting extends Logging {
  def run(spark: SparkSession, securityLevel: SecurityLevel, numPartitions: Int)
    : Long = {
    import spark.implicits._
    val inputSchema = StructType(Seq(
      StructField("src", IntegerType, false),
      StructField("dst", IntegerType, false)))
    val data = spark.read
      .schema(inputSchema)
      .option("delimiter", "\t")
      .csv(s"${Benchmark.dataDir}/web-BerkStan.txt")
    val edges =
      Utils.ensureCached(
        securityLevel.applyTo(
          data.limit(100000).repartition(numPartitions)))
    Utils.time("load edges") { Utils.force(edges) }
    logInfo(s"Loaded ${edges.count} edges")
    edges.show

    Utils.timeBenchmark(
      "distributed" -> (numPartitions > 1),
      "query" -> "triangle counting",
      "system" -> securityLevel.name) {
        edges.as("e1")
          .join(edges.as("e2"), $"e1.dst" === $"e2.src")
          .join(edges.as("e3"), $"e2.dst" === $"e3.src" && $"e3.dst" === $"e1.src")
      val result = plan.count
      logInfo(s"Counted $result triangles")
      result
    }
  }
}
