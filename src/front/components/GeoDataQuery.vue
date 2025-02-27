<template>
  <div>
    <h1>查询地理数据</h1>
    <form @submit.prevent="handleQuery">
      <label for="coords">输入坐标面数据:</label>
      <textarea id="coords" v-model="coordinates" placeholder="输入坐标数据..."></textarea>
      <button type="submit">查询数据</button>
    </form>

    <table v-if="geoData.length > 0">
      <thead>
      <tr>
        <th>ID</th>
        <th>类型</th>
        <th>坐标</th>
        <th>操作</th>
      </tr>
      </thead>
      <tbody>
      <tr v-for="(item, index) in geoData" :key="index">
        <td>{{ item.id }}</td>
        <td>{{ item.type }}</td>
        <td>{{ item.coordinates }}</td>
        <td>
          <button @click="handleDelete(item.id)">删除</button>
        </td>
      </tr>
      </tbody>
    </table>

    <button v-if="geoData.length > 0" @click="handleDownload">下载CSV</button>
    <button @click="handleRandomGenerate">随机生成数据</button>
  </div>
</template>

<script>
import { queryGeoData, downloadCSV, deleteGeoData, randomGenerate } from '@/services/geoDataService';

export default {
  data() {
    return {
      coordinates: '',   // 存储用户输入的坐标数据
      geoData: [],       // 查询到的地理信息数据
    };
  },
  methods: {
    // 处理查询操作
    handleQuery() {
      queryGeoData(this.coordinates)
          .then(response => {
            this.geoData = response.data; // 显示查询结果
          })
          .catch(error => {
            console.error('查询失败:', error);
          });
    },

    // 处理下载CSV操作
    handleDownload() {
      downloadCSV(this.geoData)
          .then(response => {
            const blob = new Blob([response.data], { type: 'text/csv' });
            const link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = 'geo_data.csv';
            link.click();
          })
          .catch(error => {
            console.error('下载失败:', error);
          });
    },

    // 处理删除数据操作
    handleDelete(id) {
      deleteGeoData(id)
          .then(response => {
            this.geoData = this.geoData.filter(item => item.id !== id);
          })
          .catch(error => {
            console.error('删除失败:', error);
          });
    },

    // 处理随机生成数据操作
    handleRandomGenerate() {
      const randomNumber = Math.floor(Math.random() * 100) + 1; // 随机生成1到100之间的数字
      randomGenerate(randomNumber)
          .then(response => {
            console.log(`成功生成 ${randomNumber} 条数据`);
          })
          .catch(error => {
            console.error('生成数据失败:', error);
          });
    },
  },
};
</script>
