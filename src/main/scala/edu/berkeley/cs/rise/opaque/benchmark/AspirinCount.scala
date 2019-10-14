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
import edu.berkeley.cs.rise.opaque.implicits._
import org.apache.spark.sql.DataFrame
import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.functions._
import org.apache.spark.sql.types._

object AspirinCount {
  def run(spark: SparkSession): DataFrame = {
    import spark.implicits._
    val diagSchema = StructType(Seq(
      StructField("patient_id", IntegerType, false),
      StructField("a", IntegerType, false),
      StructField("b", IntegerType, false),
      StructField("c", IntegerType, false),
      StructField("d", IntegerType, false),
      StructField("e", IntegerType, false),
      StructField("f", IntegerType, false),
      StructField("g", IntegerType, false),
      StructField("diagnosis", IntegerType, false),
      StructField("h", IntegerType, false),
      StructField("date", IntegerType, false),
      StructField("j", IntegerType, false),
      StructField("k", IntegerType, false)))
    val leftDiagnosis = spark.read
      .schema(diagSchema)
      .option("delimiter", ",")
      .csv(s"/home/ubuntu/aspirin_data/conclave/500000/left_diagnosis.csv")
      .select($"patient_id", $"diagnosis", $"date")
    val rightDiagnosis = spark.read
      .schema(diagSchema)
      .option("delimiter", ",")
      .csv(s"/home/ubuntu/aspirin_data/conclave/500000/right_diagnosis.csv")
      .select($"patient_id", $"diagnosis", $"date")
    val medSchema = StructType(Seq(
      StructField("patient_id", IntegerType, false),
      StructField("a", IntegerType, false),
      StructField("b", IntegerType, false),
      StructField("c", IntegerType, false),
      StructField("medication", IntegerType, false),
      StructField("d", IntegerType, false),
      StructField("e", IntegerType, false),
      StructField("date", IntegerType, false)))
    val leftMedication = spark.read
      .schema(medSchema)
      .option("delimiter", ",")
      .csv(s"/home/ubuntu/aspirin_data/conclave/500000/left_medication.csv")
      .select($"patient_id", $"medication", $"date")
    val rightMedication = spark.read
      .schema(medSchema)
      .option("delimiter", ",")
      .csv(s"/home/ubuntu/aspirin_data/conclave/500000/right_medication.csv")
      .select($"patient_id", $"medication", $"date")

    val diagnosis = leftDiagnosis.union(rightDiagnosis)
    val medication = leftMedication.union(rightMedication)
    Utils.timeBenchmark("query" -> "aspirin") {
      val result = diagnosis.filter($"diagnosis" === lit(1)).join(
        medication.filter($"medication" === lit(1)),
        diagnosis("patient_id") === medication("patient_id"))
        .encrypted
        .select(
          diagnosis("patient_id"),
          (diagnosis("date") <= medication("date")).cast("int").as("keep"))
        .groupBy(diagnosis("patient_id")).agg(sum("keep"))
      Utils.force(result)
      result
    }
  }
}
