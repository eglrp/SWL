import numpy as np

#%%------------------------------------------------------------------

class NeuralNetEvaluator(object):
	def __init__(self, neuralNet):
		self._neuralNet = neuralNet

	def evaluate(self, session, val_data, val_labels, batch_size=None):
		loss, accuracy = self._neuralNet.loss, self._neuralNet.accuracy

		num_val_examples = val_data.shape[0]

		if batch_size is None or num_val_examples <= batch_size:
			#val_loss = loss.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(val_data, val_labels, is_training=False))
			#val_acc = accuracy.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(val_data, val_labels, is_training=False))
			val_loss, val_acc = session.run([loss, accuracy], feed_dict=self._neuralNet.get_feed_dict(val_data, val_labels, is_training=False))
		else:
			val_steps_per_epoch = (num_val_examples - 1) // batch_size + 1

			indices = np.arange(num_val_examples)
			#if shuffle:
			#	np.random.shuffle(indices)

			val_loss, val_acc = 0.0, 0.0
			for step in range(val_steps_per_epoch):
				start = step * batch_size
				end = start + batch_size
				batch_indices = indices[start:end]
				if batch_indices.size > 0:  # If batch_indices is non-empty.
					data_batch, label_batch = val_data[batch_indices,], val_labels[batch_indices,]
					if data_batch.size > 0 and label_batch.size > 0:  # If data_batch and label_batch are non-empty.
						#batch_loss = loss.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(data_batch, label_batch, is_training=False))
						#batch_acc = accuracy.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(data_batch, label_batch, is_training=False))
						batch_loss, batch_acc = session.run([loss, accuracy], feed_dict=self._neuralNet.get_feed_dict(data_batch, label_batch, is_training=False))
	
						# TODO [check] >> Is val_loss or val_acc correct?
						val_loss += batch_loss * batch_indices.size
						val_acc += batch_acc * batch_indices.size
			val_loss /= num_val_examples
			val_acc /= num_val_examples

		return val_loss, val_acc

	def evaluate_seq2seq(self, session, test_encoder_inputs, test_decoder_inputs, test_decoder_outputs, batch_size=None):
		loss, accuracy = self._neuralNet.loss, self._neuralNet.accuracy

		num_val_examples = test_encoder_inputs.shape[0]

		if batch_size is None or num_val_examples <= batch_size:
			#val_loss = loss.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(test_encoder_inputs, test_decoder_inputs, test_decoder_outputs, is_training=False))
			#val_acc = accuracy.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(test_encoder_inputs, test_decoder_inputs, test_decoder_outputs, is_training=False))
			val_loss, val_acc = session.run([loss, accuracy], feed_dict=self._neuralNet.get_feed_dict(test_encoder_inputs, test_decoder_inputs, test_decoder_outputs, is_training=False))
		else:
			val_steps_per_epoch = (num_val_examples - 1) // batch_size + 1

			indices = np.arange(num_val_examples)
			#if shuffle:
			#	np.random.shuffle(indices)

			val_loss, val_acc = 0.0, 0.0
			for step in range(val_steps_per_epoch):
				start = step * batch_size
				end = start + batch_size
				batch_indices = indices[start:end]
				if batch_indices.size > 0:  # If batch_indices is non-empty.
					enc_input_batch, dec_input_batch, dec_output_batch = test_encoder_inputs[batch_indices,], test_decoder_inputs[batch_indices,], test_decoder_outputs[batch_indices,]
					if enc_input_batch.size > 0 and dec_input_batch.size > 0 and dec_output_batch.size > 0:  # If enc_input_batch, dec_input_batch, and dec_output_batch are non-empty.
						#batch_loss = loss.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(enc_input_batch, dec_input_batch, dec_output_batch, is_training=False))
						#batch_acc = accuracy.eval(session=session, feed_dict=self._neuralNet.get_feed_dict(enc_input_batch, dec_input_batch, dec_output_batch, is_training=False))
						batch_loss, batch_acc = session.run([loss, accuracy], feed_dict=self._neuralNet.get_feed_dict(enc_input_batch, dec_input_batch, dec_output_batch, is_training=False))
	
						# TODO [check] >> Is val_loss or val_acc correct?
						val_loss += batch_loss * batch_indices.size
						val_acc += batch_acc * batch_indices.size
			val_loss /= num_val_examples
			val_acc /= num_val_examples

		return val_loss, val_acc
