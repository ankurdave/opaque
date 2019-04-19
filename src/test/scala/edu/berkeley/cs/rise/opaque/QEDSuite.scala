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

package edu.berkeley.cs.rise.opaque

import org.apache.spark.sql.SparkSession
import org.scalatest.BeforeAndAfterAll
import org.scalatest.FunSuite

class QEDSuite extends FunSuite with BeforeAndAfterAll {
  val spark = SparkSession.builder()
    .master("local[1]")
    .appName("QEDSuite")
    .getOrCreate()

  Utils.initSQLContext(spark.sqlContext)

  override def afterAll(): Unit = {
    spark.stop()
  }

  ignore("Remote attestation") {
    val data = for (i <- 0 until 8) yield (i)
    RA.initRA(spark.sparkContext.parallelize(data, 2))
  }

  ignore("java encryption/decryption") {
    val data = Array[Byte](0, 1, 2)
    val (enclave, eid) = Utils.initEnclave()
    assert(data === Utils.decrypt(Utils.encrypt(data)))
    assert(data === Utils.decrypt(enclave.Encrypt(eid, data)))
  }

  test("sbnda") {
    import org.apache.spark.sql.DataFrame
    import org.apache.spark.sql.Column
    import edu.berkeley.cs.rise.opaque.expressions.DotProduct.dot
    import edu.berkeley.cs.rise.opaque.implicits._
    import spark.implicits._
    import org.apache.spark.sql.functions._
    import org.apache.spark.sql.types._
    val dir = "/sbnda/data/synthetic"
    val table1 = spark.read.format("csv").option("header", "true").load(s"$dir/riselab_table1.csv.gz").repartition(10).encrypted
    val table2 = spark.read.format("csv").option("header", "true").load(s"$dir/riselab_table2.csv.gz").repartition(10).encrypted
    val table3 = spark.read.format("csv").option("header", "true").load(s"$dir/riselab_table3.csv.gz").repartition(10).encrypted
    val joined = table1.join(table2.drop("DEF_IND").join(table3.drop("DEF_IND"), "CUST_REF"), "CUST_REF").drop("CUST_REF")

    /** Expand a categorical column into multiple columns, one for each category. */
    def getDummies(d: DataFrame, col: String): DataFrame = {
      // Get all distinct categories for the column
      val categories = d.select(col).distinct.collect.map(_.getString(0))
      // Create a new column for each category containing an indicator variable
      val expanded = categories.foldLeft(d) { (d2, c) =>
        d2.withColumn(
	  s"${col}_$c",
	  when(new Column(col) === c, lit(1.0)).otherwise(lit(0.0)))
      }
      // Drop the categorical column
      expanded.drop(col)
    }

    val withCategories = getDummies(getDummies(joined, "ATRR_76"), "ATRR_7")
    val data = withCategories.select(
      $"DEF_IND".cast(IntegerType).as("y_truth"),
      array(
	$"ATRR_35".cast(DoubleType),
	$"ATRR_36".cast(DoubleType),
	$"ATRR_76_cat_atrr_76_000".cast(DoubleType),
	$"ATRR_7_cat_atrr_7_000".cast(DoubleType),
	$"ATRR_7_cat_atrr_7_001".cast(DoubleType),
	$"ATRR_7_cat_atrr_7_003".cast(DoubleType),
	$"ATRR_7_cat_atrr_7_004".cast(DoubleType),
	$"ATRR_7_cat_atrr_7_005".cast(DoubleType))
	.as("x"))

    // Pre-trained logistic regression model from sklearn
    // Model coefficients
    val w = array(lit(1.08256682),  lit(0.85389337), lit(-0.30765509),  lit(3.26552064),  lit(2.43142285), lit(-2.35156746), lit(-4.00728388), lit(-5.74680168))
    // Model intercept
    val b = lit(-7.20533994)

    val predictions = data.select(
      $"y_truth", (lit(1.0) / (lit(1.0) + exp(-(dot(w, $"x") + b)))).as("prediction"))

    // Count the number of correct predictions. Expected: 955543 out of 1000000
    val numCorrect = predictions.filter(
      $"y_truth" === lit(1) && $"prediction" >= lit(0.5) ||
	$"y_truth" === lit(0) && $"prediction" < lit(0.5)).count

    assert(numCorrect === 955543)
  }
}


