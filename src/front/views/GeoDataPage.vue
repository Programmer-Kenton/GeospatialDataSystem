<template>
  <div>
    <GeoDataQuery /> <!-- 引入 GeoDataQuery 组件 -->
  </div>
</template>

<script>
import GeoDataQuery from '../components/GeoDataQuery.vue'; // 引入 GeoDataQuery 组件

export default {
  components: {
    GeoDataQuery, // 注册组件
  },
};
</script>
