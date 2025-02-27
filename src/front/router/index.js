import Vue from 'vue';
import Router from 'vue-router';
import GeoDataPage from '../views/GeoDataPage.vue';


Vue.use(Router);

export default new Router({
    routes: [
        {
            path: '/',
            name: 'GeoDataPage',
            component: GeoDataPage,
        },
    ],
});
