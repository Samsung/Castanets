/*
 * Copyright (C) 2010 Google Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package benchmarks;

/**
 * How do various ways of iterating through an array compare?
 */
public class ArrayIterationBenchmark {
    Foo[] mArray = new Foo[27];
    {
        for (int i = 0; i < mArray.length; ++i) mArray[i] = new Foo();
    }
    public void timeArrayIteration(int reps) {
        for (int rep = 0; rep < reps; ++rep) {
            int sum = 0;
            for (int i = 0; i < mArray.length; i++) {
                sum += mArray[i].mSplat;
            }
        }
    }
    public void timeArrayIterationCached(int reps) {
        for (int rep = 0; rep < reps; ++rep) {
            int sum = 0;
            Foo[] localArray = mArray;
            int len = localArray.length;
            
            for (int i = 0; i < len; i++) {
                sum += localArray[i].mSplat;
            }
        }
    }
    public void timeArrayIterationForEach(int reps) {
        for (int rep = 0; rep < reps; ++rep) {
            int sum = 0;
            for (Foo a: mArray) {
                sum += a.mSplat;
            }
        }
    }
}
