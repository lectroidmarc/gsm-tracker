/**
 *
 */

var Parameters = function (params) {
  this.params = (typeof params === 'object') ? params : {};
};

Parameters.prototype.set = function (name, value) {
  this.params[name] = value;
};

Parameters.prototype.serialize = function () {
  var array = [];

  for (var x in this.params) {
    if (this.params.hasOwnProperty(x)) {
      array.push(encodeURIComponent(x) + "=" + encodeURIComponent(this.params[x]));
    }
  }

  return array.join("&");
};

Parameters.prototype.formData = function () {
  var formData = new FormData();

  for (var x in this.params) {
    if (this.params.hasOwnProperty(x)) {
      formData.append(x, this.params[x]);
    }
  }

  return formData;
};
